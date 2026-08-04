#include <assert.h>
#include <stdio.h>
#include <time.h>

#include <raylib.h>
#include <raymath.h>

#define TILE 80
#define HALF_TILE TILE / 2.0f
#define GRID_COLS 20
#define GRID_ROWS 15
#define TILE_AS_VECTOR2                                                                                                                                        \
  (Vector2) {                                                                                                                                                  \
    TILE, TILE                                                                                                                                                 \
  }
#define VECTOR2_ZERO (Vector2){0, 0}
#define BUILD_PATH_MATRIX_MAX_ATTEMPT 50

#define ENEMY_SPAWN_DELAY_IN_SECONDS 1
#define NEXT_WAVE_SPAWN_DELAY_IN_SECONDS 5
#define ENEMIES_PER_WAVE 0
#define NOTIFICATION_DURATION 5.0f
#define WAVES_PER_LEVEL 3
#define NEXT_LEVEL_TRANSITION_TIMER 5.0f
#define HALF_NEXT_LEVEL_TRANSITION_TIMER (NEXT_LEVEL_TRANSITION_TIMER / 2.0f)
#define INITIAL_LIFE_POINTS 1000
#define INITIAL_GOLD 300

#define MYBROWN (Color){129, 89, 46, 255}
#define MYBROWN_DARK (Color){77, 53, 28, 255}
#define GRASS_GREEN (Color){46, 204, 113, 255}

#define BUTTON_LABEL_BUY "BUY"
#define BUTTON_LABEL_UPGRADE "UPGRADE"
#define SHOPPING_HEADER_LABEL "SHOPPING"
#define TOWER_UPGRADE_HEADER_LABEL "TOWER UPGRADE"

#define DEFAULT_UI_PADDING 10

#define ARRAY_LEN(a) (int)(sizeof(a) / sizeof((a)[0]))

#define MATH_MAX(a, b)                                                                                                                                         \
  ({                                                                                                                                                           \
    typeof(a) _a = (a);                                                                                                                                        \
    typeof(b) _b = (b);                                                                                                                                        \
    _a > _b ? _a : _b;                                                                                                                                         \
  })
#define MATH_MIN(a, b)                                                                                                                                         \
  ({                                                                                                                                                           \
    typeof(a) _a = (a);                                                                                                                                        \
    typeof(b) _b = (b);                                                                                                                                        \
    _a < _b ? _a : _b;                                                                                                                                         \
  })

#define ENTITY_BASE                                                                                                                                            \
  bool active;                                                                                                                                                 \
  unsigned int entityId

#define FOREACH_ACTIVE(var, arr)                                                                                                                               \
  for (typeof(&(arr)[0]) var = (arr); var < (arr) + ARRAY_LEN(arr); var++)                                                                                     \
    if (!var->active)                                                                                                                                          \
      continue;                                                                                                                                                \
    else

#define ENTITY_INDEX(ptr, arr) ((int)((ptr) - (arr)))

#define NEW_ENTITY(list) newEntity((list), ARRAY_LEN(list), sizeof((list)[0]))

// ================================================== SHARED STATE ==================================================

struct {
  float speed;

  struct {
    float timer;
    int count;
  } enemy;

  struct {
    int number;
    float timer;
    bool waitingNext;
    bool sceneLoaded;
  } level;

  struct {
    Vector2 pos;
    bool click;
    bool clickUsed;
  } mouse;

  struct {
    int number;
    float timer;
    bool waitingNext;
  } wave;
} gameState = {.speed = 100.0f};

// ================================================== ENTITY_WRAPPER ==================================================

typedef enum {
  ENTITY_WRAPPER_TYPE_TOWER,
  ENTITY_WRAPPER_TYPE_ENEMY,
  ENTITY_WRAPPER_TYPE_BULLET,
  ENTITY_WRAPPER_TYPE_COMMAND,
  ENTITY_WRAPPER_TYPE_NOTIFICATION,
} EntityWrapperType;

typedef struct {
  EntityWrapperType type;
  unsigned int id;
  int arrayIndex;
} EntityWrapper;

// ================================================== ENEMY ==================================================

#define ENEMY_STATS_SHARED_ATTRIBUTES                                                                                                                          \
  int health;                                                                                                                                                  \
  float speedMultiplier;                                                                                                                                       \
  int gold;                                                                                                                                                    \
  int damage

typedef enum {
  ENEMY_TYPE_NORMAL,
  ENEMY_TYPE_RUNNER,
  ENEMY_TYPE_TANK,
  ENEMY_TYPE_BOSS,
  ENEMY_TYPE_COUNT,
} EnemyType;

typedef struct {
  ENEMY_STATS_SHARED_ATTRIBUTES;
  float sizeMultiplier;
} EnemyStat;

EnemyStat enemyStats[] = {
    [ENEMY_TYPE_NORMAL] = {100, 1, 20, 10, 0.8f},
    [ENEMY_TYPE_RUNNER] = {50, 1.5, 20, 10, 0.7f},
    [ENEMY_TYPE_TANK] = {200, 0.5, 20, 10, 1.0f},
    [ENEMY_TYPE_BOSS] = {100, 1, 20, 10, 1.0f},
};
static_assert(ARRAY_LEN(enemyStats) == ENEMY_TYPE_COUNT);

typedef struct {
  ENTITY_BASE;

  Vector2 pos;
  Vector2 size;
  int targetIndex;
  EnemyType type;
  float angle;

  ENEMY_STATS_SHARED_ATTRIBUTES;
} Enemy;

Enemy enemies[64];

#undef ENEMY_STATS_SHARED_ATTRIBUTES

// ================================================== TOWER ==================================================

#define TOWER_STATS_SHARED_ATTRIBUTES                                                                                                                          \
  float range;                                                                                                                                                 \
  int fireRate;                                                                                                                                                \
  float damage

typedef enum {
  TOWER_TYPE_NORMAL,
  TOWER_TYPE_DOUBLE,
  TOWER_TYPE_COUNT,
} TowerType;

typedef struct {
  int cost;
  TOWER_STATS_SHARED_ATTRIBUTES;
} TowerStat;

TowerStat towerStats[] = {
    [TOWER_TYPE_NORMAL] = {100, TILE * 2.5f, 2, 20},
    [TOWER_TYPE_DOUBLE] = {200, TILE * 2.5f, 4, 15},
};
static_assert(ARRAY_LEN(towerStats) == TOWER_TYPE_COUNT);

typedef struct {
  ENTITY_BASE;

  Vector2 pos;
  Vector2 size;
  TowerType type;

  TOWER_STATS_SHARED_ATTRIBUTES;

  float bulletCooldown;
  float angle;
} Tower;
Tower towers[64];

typedef enum {
  TOWER_UPGRADE_TYPE_RANGE,
  TOWER_UPGRADE_TYPE_FIRE_RATE,
  TOWER_UPGRADE_TYPE_DAMAGE,
  TOWER_UPGRADE_TYPE_COUNT,
} TowerUpgradeType;

static const struct {
  char *label;
  int cost;
  int addend;
} UPGRADE_TOWER_METADATA[TOWER_UPGRADE_TYPE_COUNT] = {
    [TOWER_UPGRADE_TYPE_RANGE] = {"Range\n%0.2f > %0.2f", 200, TILE},
    [TOWER_UPGRADE_TYPE_FIRE_RATE] = {"Fire Rate\n%d/s > %d/s", 100, 1},
    [TOWER_UPGRADE_TYPE_DAMAGE] = {"Damage\n%0.2f > %0.2f", 100, 5},
};
static_assert(ARRAY_LEN(UPGRADE_TOWER_METADATA) == TOWER_UPGRADE_TYPE_COUNT);

#undef TOWER_STATS_SHARED_ATTRIBUTES

// ================================================== BULLET ==================================================

typedef struct {
  ENTITY_BASE;

  Vector2 pos;
  float size;
  Color color;
  EntityWrapper target;
  EntityWrapper tower;
} Bullet;
Bullet bullets[128];

// ================================================== ASSETS ==================================================

typedef enum {
  SCENE_TEXTURE_PATH_TILE,
  SCENE_TEXTURE_TERRAIN_TILE,
  SCENE_TEXTURE_EDGE_BOTTOM_TILE,
  SCENE_TEXTURE_EDGE_TOP_TILE,
  SCENE_TEXTURE_EDGE_LEFT_TILE,
  SCENE_TEXTURE_EDGE_RIGHT_TILE,
  SCENE_TEXTURE_CORNER_BL_TILE,
  SCENE_TEXTURE_CORNER_BR_TILE,
  SCENE_TEXTURE_CORNER_TL_TILE,
  SCENE_TEXTURE_CORNER_TR_TILE,
  SCENE_NOTIFICATION_TOAST,
  SCENE_TEXTURE_CORNER_COUNT
} SceneAsset;

Texture2D sceneTextures[SCENE_TEXTURE_CORNER_COUNT];
Texture2D enemyTextures[ENEMY_TYPE_COUNT];
Texture2D towerTextures[TOWER_TYPE_COUNT];

Font font;

RenderTexture2D sceneBaked;

// ================================================== PATH ==================================================

static const int dCol[] = {0, 0, +1, -1};
static const int dRow[] = {+1, -1, 0, 0};
int pathMatrix[GRID_ROWS][GRID_COLS];
Vector2 path[GRID_ROWS * GRID_COLS];
int pathCount = 0;

// ================================================== PLAYER ==================================================

struct {
  int lifePoints;
  int gold;
} player = {
    .lifePoints = INITIAL_LIFE_POINTS,
    .gold = INITIAL_GOLD,
};

// ================================================== FLOATING MENU ==================================================

typedef enum {
  FLOATING_MENU_TYPE_NONE,
  FLOATING_MENU_TYPE_SHOPPING,
  FLOATING_MENU_TYPE_PAUSE,
  FLOATING_MENU_TYPE_GAMEOVER,
  FLOATING_MENU_TYPE_TOWER_UPGRADE,
} FloatingMenuType;

struct {
  FloatingMenuType type;
  EntityWrapper entityWrapper;
} currentFloatingMenuOpen = {0};

static const struct {
  int key;
  FloatingMenuType from;
  FloatingMenuType to;
} FLOATING_MENU_TRANSITIONS[] = {
    {KEY_TAB, FLOATING_MENU_TYPE_NONE, FLOATING_MENU_TYPE_SHOPPING},         //
    {KEY_TAB, FLOATING_MENU_TYPE_SHOPPING, FLOATING_MENU_TYPE_NONE},         //
    {KEY_ESCAPE, FLOATING_MENU_TYPE_NONE, FLOATING_MENU_TYPE_PAUSE},         //
    {KEY_ESCAPE, FLOATING_MENU_TYPE_PAUSE, FLOATING_MENU_TYPE_NONE},         //
    {KEY_ESCAPE, FLOATING_MENU_TYPE_SHOPPING, FLOATING_MENU_TYPE_NONE},      //
    {KEY_ESCAPE, FLOATING_MENU_TYPE_TOWER_UPGRADE, FLOATING_MENU_TYPE_NONE}, //
};

typedef enum {
  FLOATING_MENU_ASSET_BASE,
  FLOATING_MENU_ASSET_HEADER_LEFT,
  FLOATING_MENU_ASSET_HEADER_CENTER,
  FLOATING_MENU_ASSET_HEADER_RIGHT,
  FLOATING_MENU_ASSET_BUTTON_DEFAULT,
  FLOATING_MENU_ASSET_BUTTON_DEFAULT_HOVER,
  FLOATING_MENU_ASSET_COUNT,
} FloatingMenuAsset;
Texture2D floatingMenuAssets[FLOATING_MENU_ASSET_COUNT];

// ================================================== COMMAND ==================================================

typedef enum { COMMAND_TYPE_BUY_TOWER, COMMAND_TYPE_PLACE_TOWER, COMMAND_TYPE_BUY_TOWER_UPGRADE } CommandType;

typedef struct {
  union {
    struct {
      TowerType towerType;
    } buyTower;
    struct {
      TowerType towerType;
      Vector2 pos;
    } placeTower;
    struct {
      EntityWrapper entityWrapper;
      TowerUpgradeType towerUpgradeType;
    } buyTowerUpgrade;
  };
} CommandCtx;

typedef struct {
  ENTITY_BASE;

  CommandType type;
  CommandCtx ctx;
  void (*computeCommand)(CommandCtx ctx);
} Command;

Command commands[32];

// ================================================== PLACEMENT ==================================================

typedef enum {
  PLACEMENT_CTX_TYPE_TOWER,
} PlacementCtxType;

typedef struct {
  PlacementCtxType type;

  union {
    struct {
      TowerType towerType;
    } tower;
  };
} PlacementCtx;

struct {
  bool active;
  PlacementCtx ctx;

  bool (*validatePlacement)(PlacementCtx ctx, Vector2 pos);
  void (*confirmPlacement)(PlacementCtx ctx, Vector2 pos);
  void (*drawPlacementGhost)(PlacementCtx ctx, Vector2 pos, bool canBePlaced);
} placement = {0};

// ================================================== NOTIFICATION ==================================================

typedef enum {
  NOTIFICATION_TYPE_TOAST,
} NotificationType;

typedef struct {
  ENTITY_BASE;

  NotificationType type;
  char *message;

  float duration;
} Notification;

Notification notifications[32];

// ================================================== UTILITY ==================================================

Vector2 cellCenter(int col, int row) {
  return (Vector2){col * TILE + HALF_TILE, row * TILE + HALF_TILE};
}

void *resolveEntity(EntityWrapper entityWrapper) {
#define RESOLVE_ENTITY_INTERNAL(arr)                                                                                                                           \
  do {                                                                                                                                                         \
    if (entityWrapper.arrayIndex < 0 || entityWrapper.arrayIndex >= ARRAY_LEN(arr)) {                                                                          \
      return NULL;                                                                                                                                             \
    }                                                                                                                                                          \
    typeof(arr[0]) *entity = &(arr)[entityWrapper.arrayIndex];                                                                                                 \
    return ((entity)->active && (entity)->entityId == entityWrapper.id) ? entity : NULL;                                                                       \
  } while (0);

  switch (entityWrapper.type) {
  case ENTITY_WRAPPER_TYPE_ENEMY:
    RESOLVE_ENTITY_INTERNAL(enemies)
  case ENTITY_WRAPPER_TYPE_TOWER:
    RESOLVE_ENTITY_INTERNAL(towers)
  case ENTITY_WRAPPER_TYPE_BULLET:
    RESOLVE_ENTITY_INTERNAL(bullets)
  case ENTITY_WRAPPER_TYPE_COMMAND:
    RESOLVE_ENTITY_INTERNAL(commands)
  case ENTITY_WRAPPER_TYPE_NOTIFICATION:
    RESOLVE_ENTITY_INTERNAL(notifications)
  }

  return NULL;
#undef RESOLVE_ENTITY_INTERNAL
}

int nextInactiveSlot(void *arr, int count, size_t stride) {
  for (int i = 0; i < count; i++) {
    bool active = *(bool *)((char *)arr + i * stride);
    if (!active) {
      return i;
    }
  }

  return -1;
}

unsigned int nextEntityId() {
  static unsigned int seq = 0;
  return ++seq;
}

void *newEntity(void *list, int count, size_t stride) {
  typedef struct {
    ENTITY_BASE;
  } EntityBase;

  int slot = nextInactiveSlot(list, count, stride);
  if (slot < 0) {
    return NULL;
  }

  EntityBase *entity = (EntityBase *)((char *)list + slot * stride);
  entity->active = true;
  entity->entityId = nextEntityId();

  return entity;
}

NPatchInfo buildNPatchInfoOnTexture(Texture texture) {
  return (NPatchInfo){
      .source = {0, 0, texture.width, texture.height},
      .left = texture.width / 2,
      .right = texture.width / 2,
      .top = texture.height / 2,
      .bottom = texture.height / 2,
      .layout = NPATCH_NINE_PATCH,
  };
}

void drawTexture(Texture2D texture, Vector2 pos, Vector2 size, float rotation, Color color) {
  Rectangle source = {0, 0, texture.width, texture.height};
  Rectangle dest = {pos.x, pos.y, size.x, size.y};
  Vector2 origin = Vector2Scale(size, 0.5f);
  DrawTexturePro(texture, source, dest, origin, rotation, color);
}

bool isClickedOn(Rectangle target) {
  if (CheckCollisionPointRec(gameState.mouse.pos, target) && gameState.mouse.click && !gameState.mouse.clickUsed) {
    gameState.mouse.clickUsed = true;
    return true;
  }

  return false;
}

// ================================================== COMMAND ==================================================

void pushCommand(CommandType type, void (*computeCommand)(CommandCtx ctx), CommandCtx ctx) {
  Command *command = NEW_ENTITY(commands);
  if (command == NULL) {
    TraceLog(LOG_ERROR, "failed to spawn command");
    return;
  }

  command->type = type;
  command->computeCommand = computeCommand;
  command->ctx = ctx;
}

void computeCommands() {
  FOREACH_ACTIVE(command, commands) {
    command->computeCommand(command->ctx);
    command->active = false;
  }
}

// ================================================== NOTIFICATION ==================================================

void pushNotification(NotificationType type, char *message) {
  Notification *notification = NEW_ENTITY(notifications);
  if (notification == NULL) {
    TraceLog(LOG_ERROR, "failed to spawn notification (%d)", type);
    return;
  }

  notification->type = type;
  notification->message = message;
  notification->duration = NOTIFICATION_DURATION;
}

void computeNotificationDuration(float dt) {
  FOREACH_ACTIVE(notification, notifications) {
    notification->duration -= dt;
    if (notification->duration <= 0) {
      notification->active = false;
    }
  }
}

void drawNotificationLine(Notification *notification, Rectangle baseDest, Vector2 textPosition, Vector2 textSize) {
  float start = baseDest.x + DEFAULT_UI_PADDING;
  float end = baseDest.x + baseDest.width - DEFAULT_UI_PADDING;
  float remaining = notification->duration / NOTIFICATION_DURATION;

  Vector2 startPos = (Vector2){
      end - ((end - start) * remaining),
      textPosition.y + textSize.y + DEFAULT_UI_PADDING,
  };
  Vector2 endPos = (Vector2){
      baseDest.x + baseDest.width - DEFAULT_UI_PADDING,
      textPosition.y + textSize.y + DEFAULT_UI_PADDING,
  };
  DrawLineEx(startPos, endPos, 3, MYBROWN_DARK);
}

float drawNotification(Notification *notification, float drawAt) {
  Vector2 textSize = MeasureTextEx(font, notification->message, font.baseSize, 1);

  Texture texture = sceneTextures[SCENE_NOTIFICATION_TOAST];
  NPatchInfo nPatchInfo = buildNPatchInfoOnTexture(texture);

  Vector2 size = (Vector2){
      textSize.x + 2 * DEFAULT_UI_PADDING,
      textSize.y + 3 * DEFAULT_UI_PADDING,
  };
  Rectangle baseDest = {
      .x = GRID_COLS * TILE - size.x - DEFAULT_UI_PADDING,
      .y = drawAt + DEFAULT_UI_PADDING,
      .width = size.x,
      .height = size.y,
  };
  DrawTextureNPatch(texture, nPatchInfo, baseDest, VECTOR2_ZERO, 0, WHITE);

  Vector2 textPosition = (Vector2){
      baseDest.x + DEFAULT_UI_PADDING,
      baseDest.y + DEFAULT_UI_PADDING,
  };
  DrawTextEx(font, notification->message, textPosition, font.baseSize, 1, MYBROWN_DARK);

  drawNotificationLine(notification, baseDest, textPosition, textSize);

  return baseDest.y + size.y;
}

void drawNotifications() {
  float drawAt = TILE;
  FOREACH_ACTIVE(notification, notifications) {
    drawAt = drawNotification(notification, drawAt);
  }
}

// ================================================== SCENE ==================================================

void resetPathMatrix() {
  for (int row = 0; row < GRID_ROWS; row++) {
    for (int col = 0; col < GRID_COLS; col++) {
      pathMatrix[row][col] = 0;
    }
  }
}

int countNeighbors(int row, int col) {
  int count = 0;
  for (int i = 0; i < 4; i++) {
    int nRow = row + dRow[i];
    int nCol = col + dCol[i];

    if (nCol < 0 || nCol >= GRID_COLS) {
      continue;
    }

    if (nRow < 0 || nRow >= GRID_ROWS) {
      continue;
    }

    if (pathMatrix[nRow][nCol]) {
      count++;
    }
  }

  return count;
}

void buildPathMatrix() {
  typedef struct {
    int row;
    int col;
  } Pos;

  static const Pos startPos = (Pos){2, 0};
  static const Pos firstPathPos = (Pos){2, 1};

  static const Pos endPos = (Pos){GRID_ROWS - 3, GRID_COLS - 1};

  static const Pos hTargets[] = {
      {3, 16},
      {8, 9},
  };
  static const Pos vTargets[] = {
      {11, 2},
      {4, 12},
  };
  static_assert(ARRAY_LEN(hTargets) == ARRAY_LEN(vTargets));

  Pos targets[ARRAY_LEN(hTargets) + 1];
  for (int i = 0; i < ARRAY_LEN(targets) - 1; i++) {
    targets[i] = gameState.wave.number % 2 == 0 ? vTargets[i] : hTargets[i];
  }

  targets[ARRAY_LEN(targets) - 1] = (Pos){GRID_ROWS - 3, GRID_COLS - 2};

  resetPathMatrix();

  pathMatrix[startPos.row][startPos.col] = 1;
  pathMatrix[firstPathPos.row][firstPathPos.col] = 1;

  Pos curPos = (Pos){firstPathPos.row, firstPathPos.col};

  int attempts = BUILD_PATH_MATRIX_MAX_ATTEMPT;
  int targetIndex = 0;
  Pos target = targets[targetIndex++];

  do {
    int direction = GetRandomValue(0, 3);
    Pos nextPos = (Pos){curPos.row + dRow[direction], curPos.col + dCol[direction]};

    int neighborsCount = countNeighbors(nextPos.row, nextPos.col);

    if (nextPos.row <= firstPathPos.row || nextPos.row >= GRID_ROWS - 2 || nextPos.col < 0 || nextPos.col >= GRID_COLS || neighborsCount > 1) {
      attempts--;
      continue;
    }

    curPos = nextPos;
    pathMatrix[curPos.row][curPos.col] = 1;
    attempts = BUILD_PATH_MATRIX_MAX_ATTEMPT;

    if (curPos.row == target.row && curPos.col == target.col) {
      if (targetIndex >= ARRAY_LEN(targets)) {
        break;
      } else {
        target = targets[targetIndex++];
        if (pathMatrix[target.row][target.col]) {
          attempts = -1;
        }
      }
    }
  } while (attempts > 0);

  if (attempts <= 0 || pathMatrix[endPos.row][endPos.col] || countNeighbors(endPos.row, endPos.col) > 1) {
    buildPathMatrix();
  } else {
    pathMatrix[endPos.row][endPos.col] = 1;
  }
}

void buildPath() {
  pathCount = 0;

  int prevCol = -1, prevRow = -1;
  int col = 0, row = 2;

  path[pathCount++] = cellCenter(col, row);

  do {
    bool moved = false;

    for (int d = 0; d < 4; d++) {
      int nCol = col + dCol[d];
      int nRow = row + dRow[d];

      if (nCol < 0 || nCol >= GRID_COLS) {
        continue;
      }
      if (nRow < 0 || nRow >= GRID_ROWS) {
        continue;
      }
      if (pathMatrix[nRow][nCol] != 1) {
        continue;
      }
      if (nCol == prevCol && nRow == prevRow) {
        continue;
      }

      prevCol = col;
      prevRow = row;
      col = nCol;
      row = nRow;
      path[pathCount++] = cellCenter(col, row);
      moved = true;
      break;
    }

    if (!moved) {
      break;
    }
  } while (pathCount < ARRAY_LEN(path));
}

void drawOnTile(int row, int col, int textureIndex) {
  Vector2 pos = cellCenter(col, row);
  drawTexture(sceneTextures[textureIndex], pos, TILE_AS_VECTOR2, 0, WHITE);
}

bool isNotRoad(int row, int col) {
  if (row < 0 || row >= GRID_ROWS || col < 0 || col >= GRID_COLS) {
    return true;
  }

  return !pathMatrix[row][col];
}

void drawScene() {
  for (int row = 0; row < GRID_ROWS; row++) {
    for (int col = 0; col < GRID_COLS; col++) {
      if (isNotRoad(row, col)) {
        drawOnTile(row, col, SCENE_TEXTURE_TERRAIN_TILE);
        continue;
      }

      drawOnTile(row, col, SCENE_TEXTURE_PATH_TILE);

      bool hasNoRoadUp = isNotRoad(row - 1, col);
      bool hasNoRoadDown = isNotRoad(row + 1, col);
      bool hasNoRoadLeft = isNotRoad(row, col - 1);
      bool hasNoRoadRight = isNotRoad(row, col + 1);

      if (hasNoRoadUp) {
        drawOnTile(row, col, SCENE_TEXTURE_EDGE_TOP_TILE);
      }

      if (hasNoRoadDown) {
        drawOnTile(row, col, SCENE_TEXTURE_EDGE_BOTTOM_TILE);
      }
      if (hasNoRoadLeft) {
        drawOnTile(row, col, SCENE_TEXTURE_EDGE_LEFT_TILE);
      }
      if (hasNoRoadRight) {
        drawOnTile(row, col, SCENE_TEXTURE_EDGE_RIGHT_TILE);
      }

      if (hasNoRoadUp && hasNoRoadLeft) {
        drawOnTile(row, col, SCENE_TEXTURE_CORNER_TL_TILE);
      }
      if (hasNoRoadUp && hasNoRoadRight) {
        drawOnTile(row, col, SCENE_TEXTURE_CORNER_TR_TILE);
      }
      if (hasNoRoadDown && hasNoRoadLeft) {
        drawOnTile(row, col, SCENE_TEXTURE_CORNER_BL_TILE);
      }
      if (hasNoRoadDown && hasNoRoadRight) {
        drawOnTile(row, col, SCENE_TEXTURE_CORNER_BR_TILE);
      }
    }
  }
}

void bakeScene() {
  sceneBaked = LoadRenderTexture(GRID_COLS * TILE, GRID_ROWS * TILE);
  BeginTextureMode(sceneBaked);
  ClearBackground(RAYWHITE);
  drawScene();
  EndTextureMode();
}

void drawBakedScene() {
  Rectangle rec = (Rectangle){0, 0, sceneBaked.texture.width, -sceneBaked.texture.height};
  DrawTextureRec(sceneBaked.texture, rec, VECTOR2_ZERO, RAYWHITE);
}

// ================================================== WAVE ==================================================

bool isWaveEnded() {
  FOREACH_ACTIVE(enemy, enemies) return false;
  return true;
}

void computeWaveState(float dt) {
  if (gameState.level.waitingNext) {
    return;
  }

  if (gameState.enemy.count > (ENEMIES_PER_WAVE * gameState.wave.number)) {
    gameState.wave.waitingNext = true;
  }

  if (gameState.wave.waitingNext && isWaveEnded() && gameState.wave.timer < 0) {
    gameState.wave.timer = 0;
  }

  if (gameState.wave.timer >= 0) {
    gameState.wave.timer += dt;
    if (gameState.wave.timer >= NEXT_WAVE_SPAWN_DELAY_IN_SECONDS) {
      gameState.wave.timer = -1;
      gameState.wave.number++;
      gameState.wave.waitingNext = false;
      gameState.enemy.count = 0;
    }
  }
}

// ================================================== ENEMY ==================================================

void spawnEnemy() {
  Enemy *enemy = NEW_ENTITY(enemies);
  if (enemy == NULL) {
    return;
  }

  EnemyType enemyType = GetRandomValue(0, ENEMY_TYPE_COUNT - 2);
  EnemyStat defaultStats = enemyStats[enemyType];
  float statMultiplier = gameState.level.number / 10.0f + MATH_MAX(gameState.wave.number / 5.0f, 1.0f);

  enemy->type = enemyType;
  enemy->pos = path[0];
  enemy->targetIndex = 1;
  enemy->size = Vector2Scale(TILE_AS_VECTOR2, defaultStats.sizeMultiplier);
  enemy->health = defaultStats.health * statMultiplier;
  enemy->speedMultiplier = defaultStats.speedMultiplier;
  enemy->gold = defaultStats.gold;
  enemy->damage = defaultStats.damage * statMultiplier;

  gameState.enemy.count++;
}

void computeSpawnEnemy(float dt) {
  if (gameState.wave.waitingNext) {
    return;
  }

  gameState.enemy.timer += dt;
  if (gameState.enemy.timer >= ENEMY_SPAWN_DELAY_IN_SECONDS) {
    gameState.enemy.timer -= ENEMY_SPAWN_DELAY_IN_SECONDS;
    spawnEnemy();
  }
}

void computeEnemyHit(Enemy *enemy) {
  enemy->active = false;
  player.lifePoints -= enemy->damage;
}

void computeEnemiesMovement(float dt) {
  FOREACH_ACTIVE(enemy, enemies) {
    Vector2 target = path[enemy->targetIndex];
    enemy->pos = Vector2MoveTowards(enemy->pos, target, (gameState.speed * enemy->speedMultiplier) * dt);
    if (Vector2Equals(enemy->pos, target)) {
      enemy->targetIndex++;
    }

    if (enemy->targetIndex == pathCount) {
      computeEnemyHit(enemy);
      continue;
    }

    Vector2 newTarget = path[enemy->targetIndex];
    Vector2 dir = Vector2Subtract(newTarget, enemy->pos);
    enemy->angle = atan2f(dir.y, dir.x) * RAD2DEG;
  }
}

void drawEnemy(Enemy *enemy) {
  drawTexture(enemyTextures[enemy->type], enemy->pos, enemy->size, enemy->angle, WHITE);
}

// ================================================== TOWER ==================================================

bool retrieveTowerTarget(Tower *tower, EntityWrapper *out) {
  struct {
    float distance;
    Enemy *enemy;
    bool found;
  } closest = {
      .distance = tower->range * 2,
      .found = false,
  };

  FOREACH_ACTIVE(enemy, enemies) {
    float distance = Vector2Distance(tower->pos, enemy->pos);
    if (distance < tower->range && distance < closest.distance) {
      closest.distance = distance;
      closest.enemy = enemy;
      closest.found = true;
    }
  }

  if (closest.found) {
    out->type = ENTITY_WRAPPER_TYPE_ENEMY;
    out->id = closest.enemy->entityId;
    out->arrayIndex = ENTITY_INDEX(closest.enemy, enemies);
    return true;
  }

  return false;
}

void spawnTower(TowerType towerType, Vector2 pos) {
  Tower *tower = NEW_ENTITY(towers);
  if (tower == NULL) {
    TraceLog(LOG_ERROR, "failed to spawn tower");
    return;
  }

  tower->pos = pos;
  tower->size = Vector2Scale(TILE_AS_VECTOR2, 0.8f);
  tower->type = towerType;

  tower->range = towerStats[towerType].range;
  tower->fireRate = towerStats[towerType].fireRate;
  tower->damage = towerStats[towerType].damage;

  tower->bulletCooldown = 1.0f / tower->fireRate;
  tower->angle = 0;

  TraceLog(LOG_DEBUG, "tower (%d) spawned (%f %f)", towerType, tower->pos.x, tower->pos.y);
}

void computeTowerAngle() {
  FOREACH_ACTIVE(tower, towers) {
    EntityWrapper entityWrapper;
    if (!retrieveTowerTarget(tower, &entityWrapper)) {
      continue;
    }

    Enemy *enemy = resolveEntity(entityWrapper);
    if (enemy == NULL) {
      continue;
    }

    Vector2 dir = Vector2Subtract(enemy->pos, tower->pos);
    tower->angle = (atan2f(dir.y, dir.x) * RAD2DEG) + 90;
  }
}

void drawTower(Tower *tower) {
  drawTexture(towerTextures[tower->type], tower->pos, tower->size, tower->angle, WHITE);

  Rectangle towerRectangle = (Rectangle){tower->pos.x - HALF_TILE, tower->pos.y - HALF_TILE, TILE, TILE};
  if (isClickedOn(towerRectangle)) {
    currentFloatingMenuOpen.type = FLOATING_MENU_TYPE_TOWER_UPGRADE;
    currentFloatingMenuOpen.entityWrapper = (EntityWrapper){
        .type = ENTITY_WRAPPER_TYPE_TOWER,
        .id = tower->entityId,
        .arrayIndex = ENTITY_INDEX(tower, towers),
    };
  }
}

bool validateTowerPlacement(PlacementCtx ctx, Vector2 pos) {
  assert(ctx.type == PLACEMENT_CTX_TYPE_TOWER);

  int posX = pos.x / TILE;
  int posY = pos.y / TILE;

  if (posX < 0 || posX >= GRID_COLS) {
    return false;
  }

  if (posY <= 1 || posY >= GRID_ROWS - 1) {
    return false;
  }

  if (pathMatrix[posY][posX]) {
    return false;
  }

  FOREACH_ACTIVE(tower, towers) {
    int towerPosX = tower->pos.x / TILE;
    int towerPosY = tower->pos.y / TILE;
    if (towerPosX == posX && towerPosY == posY) {
      return false;
    }
  }

  return true;
}

void drawTowerPlacementGhost(PlacementCtx ctx, Vector2 pos, bool canBePlaced) {
  assert(ctx.type == PLACEMENT_CTX_TYPE_TOWER);

  pos = (Vector2){
      .x = ((int)(pos.x / TILE)) * TILE + HALF_TILE,
      .y = ((int)(pos.y / TILE)) * TILE + HALF_TILE,
  };
  Color color = canBePlaced ? Fade(GREEN, 0.7f) : Fade(RED, 0.7f);
  drawTexture(towerTextures[ctx.tower.towerType], pos, TILE_AS_VECTOR2, 0, color);
}

void computePlaceTowerCommand(CommandCtx ctx) {
  TowerType towerType = ctx.placeTower.towerType;
  Vector2 pos = ctx.placeTower.pos;

  TowerStat stat = towerStats[towerType];
  player.gold -= stat.cost;

  spawnTower(towerType, cellCenter(pos.x / TILE, pos.y / TILE));
}

void confirmTowerPlacement(PlacementCtx ctx, Vector2 pos) {
  assert(ctx.type == PLACEMENT_CTX_TYPE_TOWER);

  TowerType towerType = ctx.tower.towerType;
  if (player.gold < towerStats[towerType].cost) {
    TraceLog(LOG_ERROR, "should be unreachable");
    return;
  }

  CommandCtx commandCtx = (CommandCtx){.placeTower = {towerType, pos}};
  pushCommand(COMMAND_TYPE_PLACE_TOWER, computePlaceTowerCommand, commandCtx);
}

void computeBuyTowerCommand(CommandCtx ctx) {
  TowerType towerType = ctx.buyTower.towerType;
  currentFloatingMenuOpen.type = FLOATING_MENU_TYPE_NONE;

  TowerStat towerStat = towerStats[towerType];
  if (player.gold < towerStat.cost) {
    pushNotification(NOTIFICATION_TYPE_TOAST, "Insufficient Gold");
    return;
  }

  placement.active = true;
  placement.ctx = (PlacementCtx){.tower = {.towerType = towerType}};
  placement.validatePlacement = validateTowerPlacement;
  placement.confirmPlacement = confirmTowerPlacement;
  placement.drawPlacementGhost = drawTowerPlacementGhost;
}

int calcTowerUpgradeCost(TowerUpgradeType towerUpgradeType, Tower *tower) {
  int updates = -1;
  switch (towerUpgradeType) {
  case TOWER_UPGRADE_TYPE_RANGE:
    updates = tower->range - towerStats[tower->type].range;
    break;
  case TOWER_UPGRADE_TYPE_FIRE_RATE:
    updates = tower->fireRate - towerStats[tower->type].fireRate;
    break;
  case TOWER_UPGRADE_TYPE_DAMAGE:
    updates = tower->damage - towerStats[tower->type].damage;
    break;
  case TOWER_UPGRADE_TYPE_COUNT:
    break;
  }

  assert(updates >= 0);

  updates = updates / UPGRADE_TOWER_METADATA[towerUpgradeType].addend + 1;

  return UPGRADE_TOWER_METADATA[towerUpgradeType].cost * updates;
}

void computeBuyTowerUpgradeCommand(CommandCtx ctx) {
  Tower *tower = resolveEntity(ctx.buyTowerUpgrade.entityWrapper);
  if (tower == NULL) {
    TraceLog(LOG_ERROR, "could not resolve tower");
    return;
  }

  TowerUpgradeType towerUpgradeType = ctx.buyTowerUpgrade.towerUpgradeType;

  int cost = calcTowerUpgradeCost(towerUpgradeType, tower);
  if (player.gold < cost) {
    pushNotification(NOTIFICATION_TYPE_TOAST, "Insufficient Gold");
    return;
  }

  player.gold -= cost;

  float multiplier = UPGRADE_TOWER_METADATA[towerUpgradeType].addend;
  switch (towerUpgradeType) {
  case TOWER_UPGRADE_TYPE_RANGE:
    tower->range += multiplier;
    break;
  case TOWER_UPGRADE_TYPE_FIRE_RATE:
    tower->fireRate += multiplier;
    break;
  case TOWER_UPGRADE_TYPE_DAMAGE:
    tower->damage += multiplier;
    break;
  case TOWER_UPGRADE_TYPE_COUNT:
    break;
  }

  pushNotification(NOTIFICATION_TYPE_TOAST, "Tower Upgraded");
}

void sellTower(Tower *tower) {
  tower->active = false;
  player.gold += towerStats[tower->type].cost * 0.33f;
}

void sellAllTowers() {
  FOREACH_ACTIVE(tower, towers) sellTower(tower);
}

// ================================================== BULLET ==================================================

void spawnBullet(Tower *tower, EntityWrapper target) {
  Bullet *bullet = NEW_ENTITY(bullets);
  if (bullet == NULL) {
    return;
  }

  bullet->pos = tower->pos;
  bullet->size = 5.0f;
  bullet->color = BLACK;
  bullet->target = target;
  bullet->tower = (EntityWrapper){
      .type = ENTITY_WRAPPER_TYPE_TOWER,
      .id = tower->entityId,
      .arrayIndex = ENTITY_INDEX(tower, towers),
  };
}

void computeSpawnBullet(float dt) {
  FOREACH_ACTIVE(tower, towers) {
    tower->bulletCooldown += dt;
    float fireInterval = 1.0f / tower->fireRate;
    if (tower->bulletCooldown < fireInterval) {
      continue;
    }
    tower->bulletCooldown -= fireInterval;

    EntityWrapper entityWrapper;
    if (!retrieveTowerTarget(tower, &entityWrapper)) {
      continue;
    }
    spawnBullet(tower, entityWrapper);
  }
}

void computeBulletHit(Bullet *bullet) {
  Enemy *enemy = resolveEntity(bullet->target);
  Tower *tower = resolveEntity(bullet->tower);

  if (enemy == NULL || tower == NULL) {
    bullet->active = false;
    return;
  }

  bullet->active = false;
  enemy->health -= tower->damage;

  if (enemy->health <= 0) {
    enemy->active = false;
    player.gold += enemy->gold;
  }
}

void computeBulletsMovement(float dt) {
  FOREACH_ACTIVE(bullet, bullets) {
    Enemy *enemy = resolveEntity(bullet->target);
    if (enemy == NULL) {
      bullet->active = false;
      continue;
    }

    Vector2 target = enemy->pos;
    bullet->pos = Vector2MoveTowards(bullet->pos, target, gameState.speed * 4 * dt);
    if (Vector2Equals(bullet->pos, target)) {
      computeBulletHit(bullet);
    }
  }
}

void drawBullet(Bullet *bullet) {
  DrawCircleV(bullet->pos, bullet->size, bullet->color);
}

void computeBullet(float dt) {
  computeSpawnBullet(dt);
  computeBulletsMovement(dt);
}

// ================================================== LEVEL ==================================================

void computeLevelState(float dt) {
  if (gameState.wave.waitingNext && gameState.wave.number >= WAVES_PER_LEVEL) {
    gameState.level.waitingNext = true;
  }

  if (gameState.level.waitingNext && isWaveEnded() && gameState.level.timer < 0) {
    gameState.level.sceneLoaded = false;
    gameState.level.timer = 0;
  }

  if (gameState.level.timer >= 0) {
    gameState.level.timer += dt;

    if (gameState.level.timer >= HALF_NEXT_LEVEL_TRANSITION_TIMER && !gameState.level.sceneLoaded) {
      buildPathMatrix();
      buildPath();
      bakeScene();

      gameState.level.sceneLoaded = true;
      gameState.wave.number = 0;

      sellAllTowers();
    }

    if (gameState.level.timer >= NEXT_LEVEL_TRANSITION_TIMER) {
      gameState.level.number++;
      gameState.level.waitingNext = false;
      gameState.level.timer = -1;

      player.gold += (INITIAL_GOLD * gameState.level.number) / 2;
    }
  }
}

void drawLevelTransition() {
  if (!gameState.level.waitingNext) {
    return;
  }

  Rectangle rec = (Rectangle){
      .x = 0,
      .y = TILE * GRID_ROWS * ((gameState.level.timer - HALF_NEXT_LEVEL_TRANSITION_TIMER) / HALF_NEXT_LEVEL_TRANSITION_TIMER),
      .width = TILE * GRID_COLS,
      .height = TILE * GRID_ROWS,
  };

  DrawRectanglePro(rec, VECTOR2_ZERO, 0, GRASS_GREEN);
}

// ================================================== HUD ==================================================

float drawStat(float x, const char *msg) {
  Vector2 size = MeasureTextEx(font, msg, font.baseSize, 1);
  DrawTextEx(font, msg, (Vector2){x, (TILE - size.y) / 2}, font.baseSize, 1, RAYWHITE);
  return x + size.x + 15;
}

void drawHud() {
  int maxWidth = GetScreenWidth();
  DrawRectangle(0, 0, maxWidth, TILE, MYBROWN);

  float x = 10;
  x = drawStat(x, TextFormat("Life: %d", player.lifePoints));
  x = drawStat(x, TextFormat("Wave: %d", gameState.wave.number));
  x = drawStat(x, TextFormat("Gold: %d", player.gold));
  if (isWaveEnded()) {
    if (gameState.wave.timer > 0) {
      x = drawStat(x, TextFormat("Next Wave In: %.0f", (NEXT_WAVE_SPAWN_DELAY_IN_SECONDS - gameState.wave.timer)));
    }
  } else {
    int remainingEnemies = ENEMIES_PER_WAVE * gameState.wave.number - gameState.enemy.count + 1;
    x = drawStat(x, TextFormat("Remaining Enemies To Spawn: %d", remainingEnemies));
  }

  int maxHeight = GetScreenHeight();
  DrawRectangle(0, maxHeight - TILE, maxWidth, TILE, MYBROWN);
}

// ================================================== FLOATING MENU ==================================================

void computeFloatingMenuKeys() {
  int keyPressed = GetKeyPressed();
  if (keyPressed == KEY_NULL) {
    return;
  }

  if (keyPressed == KEY_ESCAPE && placement.active) {
    placement.active = false;
    return;
  }

  for (int i = 0; i < ARRAY_LEN(FLOATING_MENU_TRANSITIONS); i++) {
    if (keyPressed == FLOATING_MENU_TRANSITIONS[i].key && currentFloatingMenuOpen.type == FLOATING_MENU_TRANSITIONS[i].from) {
      placement.active = false;
      currentFloatingMenuOpen.type = FLOATING_MENU_TRANSITIONS[i].to;
      return;
    }
  }
}

void drawFloatingMenuHeaderStrip(Rectangle baseDest) {
  Texture centerTexture = floatingMenuAssets[FLOATING_MENU_ASSET_HEADER_CENTER];
  Texture leftTexture = floatingMenuAssets[FLOATING_MENU_ASSET_HEADER_LEFT];
  Texture rightTexture = floatingMenuAssets[FLOATING_MENU_ASSET_HEADER_RIGHT];
  assert(centerTexture.width == leftTexture.width && centerTexture.width == rightTexture.width);
  assert(centerTexture.height == leftTexture.height && centerTexture.height == rightTexture.height);

  Vector2 floatingTileSize = {centerTexture.width, centerTexture.height};

  Vector2 origin = Vector2Scale(floatingTileSize, 0.5f);
  Rectangle source = {0, 0, floatingTileSize.x, floatingTileSize.y};

  Rectangle leftDest = {baseDest.x + 1.5 * TILE, baseDest.y, floatingTileSize.x, floatingTileSize.y};
  DrawTexturePro(leftTexture, source, leftDest, origin, 0, RAYWHITE);

  Rectangle rightDest = {baseDest.x + baseDest.width - 1.5 * TILE, baseDest.y, floatingTileSize.x, floatingTileSize.y};
  DrawTexturePro(rightTexture, source, rightDest, origin, 0, RAYWHITE);

  float cur = leftDest.x + leftDest.width;
  while (cur < rightDest.x) {
    Rectangle dest = {cur, baseDest.y, MATH_MIN(floatingTileSize.x, rightDest.x - cur), floatingTileSize.y};
    DrawTexturePro(centerTexture, source, dest, origin, 0, RAYWHITE);

    cur += floatingTileSize.x;
  };
}

Vector2 drawRawFloatingMenu(Vector2 sizeInTiles, char *headerText) {
  assert(sizeInTiles.x > 5 && sizeInTiles.x < GRID_COLS && sizeInTiles.y > 0 && sizeInTiles.y < GRID_ROWS);

  Texture texture = floatingMenuAssets[FLOATING_MENU_ASSET_BASE];

  NPatchInfo nPatchInfo = buildNPatchInfoOnTexture(texture);

  Rectangle baseDest = {
      .x = (GRID_COLS - sizeInTiles.x) / 2 * TILE,
      .y = (GRID_ROWS - sizeInTiles.y) / 2 * TILE,
      .width = TILE * sizeInTiles.x,
      .height = TILE * sizeInTiles.y,
  };

  DrawTextureNPatch(texture, nPatchInfo, baseDest, VECTOR2_ZERO, 0, WHITE);

  drawFloatingMenuHeaderStrip(baseDest);

  Vector2 textSize = MeasureTextEx(font, headerText, font.baseSize, 1);
  DrawTextEx(font, headerText, (Vector2){baseDest.x + baseDest.width / 2 - textSize.x / 2, baseDest.y - textSize.y / 2}, font.baseSize, 1, RAYWHITE);

  return (Vector2){baseDest.x + TILE, baseDest.y + TILE * 0.75};
}

void drawPauseFloatingMenu() {
  assert(currentFloatingMenuOpen.type == FLOATING_MENU_TYPE_PAUSE);

  Vector2 size = {6, 3};
  drawRawFloatingMenu(size, "PAUSE");
  // TODO: improve
}

void drawBuyTowerWidgetDescription(TowerType towerType, Texture towerTexture, Vector2 drawAt) {
  TowerStat towerStat = towerStats[towerType];
  const char *lines[] = {
      TextFormat("Cost: $%d", towerStat.cost),
      TextFormat("Range: %0.2f", towerStat.range),
      TextFormat("Fire Rate: %d/s", towerStat.fireRate),
      TextFormat("Damage: %0.2f", towerStat.damage),
  };
  Vector2 textPos = {
      .x = drawAt.x + towerTexture.width + DEFAULT_UI_PADDING,
      .y = drawAt.y + DEFAULT_UI_PADDING * 1.5,
  };
  for (int i = 0; i < ARRAY_LEN(lines); i++) {
    const char *msg = lines[i];
    Vector2 size = MeasureTextEx(font, msg, font.baseSize, 1);
    DrawTextEx(font, msg, textPos, font.baseSize, 1, MYBROWN_DARK);
    textPos.y += size.y + DEFAULT_UI_PADDING;
  }
}

Rectangle drawFloatingMenuButton(Rectangle rectangleWrapper, const char *label) {
  Vector2 textSize = MeasureTextEx(font, label, font.baseSize, 1);

  Vector2 size = (Vector2){
      .x = textSize.x + 3 * DEFAULT_UI_PADDING,
      .y = textSize.y + 3 * DEFAULT_UI_PADDING,
  };

  Rectangle rectangle = {
      .x = rectangleWrapper.x + rectangleWrapper.width - size.x - DEFAULT_UI_PADDING,
      .y = rectangleWrapper.y + rectangleWrapper.height - size.y - DEFAULT_UI_PADDING,
      .width = size.x,
      .height = size.y,
  };

  bool isMouseOver = CheckCollisionPointRec(GetMousePosition(), rectangle);
  Texture texture = floatingMenuAssets[isMouseOver ? FLOATING_MENU_ASSET_BUTTON_DEFAULT_HOVER : FLOATING_MENU_ASSET_BUTTON_DEFAULT];
  NPatchInfo nPatchInfo = buildNPatchInfoOnTexture(texture);
  DrawTextureNPatch(texture, nPatchInfo, rectangle, VECTOR2_ZERO, 0, WHITE);

  Vector2 textPos = {
      .x = rectangle.x + rectangle.width / 2 - textSize.x / 2,
      .y = rectangle.y + rectangle.height / 2 - textSize.y / 2,
  };
  DrawTextEx(font, label, textPos, font.baseSize, 1, WHITE);

  return rectangle;
}

Rectangle drawBuyTowerWidgetBuyButton(Rectangle rectangleWrapper) {
  return drawFloatingMenuButton(rectangleWrapper, BUTTON_LABEL_BUY);
}

Vector2 drawBuyTowerWidget(TowerType towerType, Vector2 size, Vector2 drawAt) {
  Texture2D towerTexture = towerTextures[towerType];
  DrawTexture(towerTexture, drawAt.x, drawAt.y, RAYWHITE);

  Rectangle rectangleWrapper = (Rectangle){
      .x = drawAt.x,
      .y = drawAt.y,
      .width = (size.x - 2) * TILE,
      .height = towerTexture.height,
  };
  DrawRectangleRoundedLinesEx(rectangleWrapper, 0.1, 1, 2, MYBROWN);

  Rectangle buttonRectangle = drawBuyTowerWidgetBuyButton(rectangleWrapper);

  drawBuyTowerWidgetDescription(towerType, towerTexture, drawAt);

  if (isClickedOn(buttonRectangle)) {
    CommandCtx commandCtx = (CommandCtx){.buyTower = {.towerType = towerType}};
    pushCommand(COMMAND_TYPE_BUY_TOWER, computeBuyTowerCommand, commandCtx);
  }

  return (Vector2){.x = drawAt.x, .y = drawAt.y + towerTexture.height + DEFAULT_UI_PADDING};
}

void drawShoppingFloatingMenu() {
  assert(currentFloatingMenuOpen.type == FLOATING_MENU_TYPE_SHOPPING);

  Vector2 size = {8, 11};
  Vector2 drawAt = drawRawFloatingMenu(size, SHOPPING_HEADER_LABEL);

  for (int i = 0; i < TOWER_TYPE_COUNT; i++) {
    drawAt = drawBuyTowerWidget(i, size, drawAt);
  }
}

const char *buildTowerUpgradeItemText(TowerUpgradeType towerUpgradeType, Tower *tower) {
  int cost = calcTowerUpgradeCost(towerUpgradeType, tower);
  const char *text = TextFormat("%s\nCost: $%d", UPGRADE_TOWER_METADATA[towerUpgradeType].label, cost);

  switch (towerUpgradeType) {
  case TOWER_UPGRADE_TYPE_RANGE:
    return TextFormat(text, tower->range, tower->range + UPGRADE_TOWER_METADATA[towerUpgradeType].addend);
  case TOWER_UPGRADE_TYPE_FIRE_RATE:
    return TextFormat(text, tower->fireRate, tower->fireRate + UPGRADE_TOWER_METADATA[towerUpgradeType].addend);
  case TOWER_UPGRADE_TYPE_DAMAGE:
    return TextFormat(text, tower->damage, tower->damage + UPGRADE_TOWER_METADATA[towerUpgradeType].addend);
  case TOWER_UPGRADE_TYPE_COUNT:
    break;
  };

  assert(false && "should be unreachable");
}

void drawTowerUpgradeFloatingMenu() {
  assert(currentFloatingMenuOpen.type == FLOATING_MENU_TYPE_TOWER_UPGRADE);

  Vector2 size = {8, 6};
  Vector2 drawAt = drawRawFloatingMenu(size, TOWER_UPGRADE_HEADER_LABEL);

  Tower *tower = resolveEntity(currentFloatingMenuOpen.entityWrapper);
  if (tower == NULL) {
    TraceLog(LOG_ERROR, "could not resolve tower");
    return;
  }

  Texture2D texture = towerTextures[tower->type];
  Vector2 texturePos = {
      .x = drawAt.x + (size.x - 2) * HALF_TILE,
      .y = drawAt.y + HALF_TILE,
  };
  drawTexture(texture, texturePos, TILE_AS_VECTOR2, 0, WHITE);
  drawAt.y += TILE + DEFAULT_UI_PADDING * 2;

  for (int i = 0; i < TOWER_UPGRADE_TYPE_COUNT; i++) {
    const char *text = buildTowerUpgradeItemText(i, tower);

    Vector2 textSize = MeasureTextEx(font, text, font.baseSize, 1);
    Vector2 textPos = (Vector2){
        .x = drawAt.x + DEFAULT_UI_PADDING,
        .y = drawAt.y + DEFAULT_UI_PADDING,
    };
    DrawTextEx(font, text, textPos, font.baseSize, 1, MYBROWN_DARK);

    Rectangle rectangleWrapper = (Rectangle){
        .x = drawAt.x,
        .y = drawAt.y,
        .width = (size.x - 2) * TILE,
        .height = textSize.y + 2 * DEFAULT_UI_PADDING,
    };
    DrawRectangleRoundedLinesEx(rectangleWrapper, 0.1, 1, 2, MYBROWN);

    Rectangle buttonRectangle = drawFloatingMenuButton(rectangleWrapper, BUTTON_LABEL_UPGRADE);

    if (isClickedOn(buttonRectangle)) {
      CommandCtx commandCtx = (CommandCtx){.buyTowerUpgrade = {currentFloatingMenuOpen.entityWrapper, i}};
      pushCommand(COMMAND_TYPE_BUY_TOWER_UPGRADE, computeBuyTowerUpgradeCommand, commandCtx);
      currentFloatingMenuOpen.type = FLOATING_MENU_TYPE_NONE;
    }

    drawAt.y += rectangleWrapper.height + DEFAULT_UI_PADDING;
  }
}

void drawFloatingMenu() {
  switch (currentFloatingMenuOpen.type) {
  case FLOATING_MENU_TYPE_NONE:
    break;
  case FLOATING_MENU_TYPE_SHOPPING:
    drawShoppingFloatingMenu();
    break;
  case FLOATING_MENU_TYPE_PAUSE:
    drawPauseFloatingMenu();
    break;
  case FLOATING_MENU_TYPE_GAMEOVER:
    break;
  case FLOATING_MENU_TYPE_TOWER_UPGRADE:
    drawTowerUpgradeFloatingMenu();
    break;
  }
}

// ================================================== PLACEMENT ==================================================

void drawPlacement() {
  if (!placement.active) {
    return;
  }

  Vector2 pos = GetMousePosition();
  bool canBePlaced = placement.validatePlacement(placement.ctx, pos);
  placement.drawPlacementGhost(placement.ctx, pos, canBePlaced);
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && canBePlaced) {
    TraceLog(LOG_DEBUG, "placed (%f, %f)", pos.x, pos.y);
    placement.confirmPlacement(placement.ctx, pos);
    placement.active = false;
  }
}

// ================================================== ASSETS ==================================================

void loadAssets() {
  font = LoadFontEx("assets/fonts/PressStart2P-Regular.ttf", 16, NULL, 0);

  enemyTextures[ENEMY_TYPE_NORMAL] = LoadTexture("assets/sprites/enemy01.png");
  enemyTextures[ENEMY_TYPE_RUNNER] = LoadTexture("assets/sprites/enemy02.png");
  enemyTextures[ENEMY_TYPE_TANK] = LoadTexture("assets/sprites/enemy03.png");
  enemyTextures[ENEMY_TYPE_BOSS] = LoadTexture("assets/sprites/enemy04.png");

  towerTextures[TOWER_TYPE_NORMAL] = LoadTexture("assets/sprites/tower01.png");
  towerTextures[TOWER_TYPE_DOUBLE] = LoadTexture("assets/sprites/tower02.png");

  sceneTextures[SCENE_TEXTURE_PATH_TILE] = LoadTexture("assets/sprites/path_tile.png");
  sceneTextures[SCENE_TEXTURE_TERRAIN_TILE] = LoadTexture("assets/sprites/grass_tile.png");
  sceneTextures[SCENE_TEXTURE_EDGE_BOTTOM_TILE] = LoadTexture("assets/sprites/edge_bottom_tile.png");
  sceneTextures[SCENE_TEXTURE_EDGE_TOP_TILE] = LoadTexture("assets/sprites/edge_top_tile.png");
  sceneTextures[SCENE_TEXTURE_EDGE_LEFT_TILE] = LoadTexture("assets/sprites/edge_left_tile.png");
  sceneTextures[SCENE_TEXTURE_EDGE_RIGHT_TILE] = LoadTexture("assets/sprites/edge_right_tile.png");
  sceneTextures[SCENE_TEXTURE_CORNER_BL_TILE] = LoadTexture("assets/sprites/corner_bl_tile.png");
  sceneTextures[SCENE_TEXTURE_CORNER_BR_TILE] = LoadTexture("assets/sprites/corner_br_tile.png");
  sceneTextures[SCENE_TEXTURE_CORNER_TL_TILE] = LoadTexture("assets/sprites/corner_tl_tile.png");
  sceneTextures[SCENE_TEXTURE_CORNER_TR_TILE] = LoadTexture("assets/sprites/corner_tr_tile.png");
  sceneTextures[SCENE_NOTIFICATION_TOAST] = LoadTexture("assets/sprites/notification_toast.png");

  floatingMenuAssets[FLOATING_MENU_ASSET_BASE] = LoadTexture("assets/sprites/floating_menu_base.png");
  floatingMenuAssets[FLOATING_MENU_ASSET_HEADER_LEFT] = LoadTexture("assets/sprites/floating_menu_header_left.png");
  floatingMenuAssets[FLOATING_MENU_ASSET_HEADER_CENTER] = LoadTexture("assets/sprites/floating_menu_header_center.png");
  floatingMenuAssets[FLOATING_MENU_ASSET_HEADER_RIGHT] = LoadTexture("assets/sprites/floating_menu_header_right.png");
  floatingMenuAssets[FLOATING_MENU_ASSET_BUTTON_DEFAULT] = LoadTexture("assets/sprites/floating_menu_button_default.png");
  floatingMenuAssets[FLOATING_MENU_ASSET_BUTTON_DEFAULT_HOVER] = LoadTexture("assets/sprites/floating_menu_button_default_hover.png");
}

void unloadAssets() {
  UnloadFont(font);
  UnloadRenderTexture(sceneBaked);

  for (int i = 0; i < ARRAY_LEN(enemyTextures); i++) {
    UnloadTexture(enemyTextures[i]);
  }
  for (int i = 0; i < ARRAY_LEN(towerTextures); i++) {
    UnloadTexture(towerTextures[i]);
  }
  for (int i = 0; i < ARRAY_LEN(sceneTextures); i++) {
    UnloadTexture(sceneTextures[i]);
  }
}

// ================================================== MAIN ==================================================

void computeMouseState() {
  gameState.mouse.pos = GetMousePosition();
  gameState.mouse.click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  gameState.mouse.clickUsed = false;
}

void drawDebugGrid() {
  for (int col = 0; col <= GRID_COLS; col++) {
    int x = col * TILE;
    DrawLineDashed((Vector2){x, 0}, (Vector2){x, GRID_ROWS * TILE}, 5, 5, LIGHTGRAY);
  }
  for (int row = 0; row <= GRID_ROWS; row++) {
    int y = row * TILE;
    DrawLineDashed((Vector2){0, y}, (Vector2){GRID_COLS * TILE, y}, 5, 5, LIGHTGRAY);
  }
  for (int i = 0; i < ARRAY_LEN(towers); i++) {
    Tower tower = towers[i];
    if (tower.active) {
      DrawCircleLinesV(tower.pos, tower.range, LIGHTGRAY);
    }
  }

  int fontSize = GetFontDefault().baseSize;
  const char *msg = TextFormat("(%d, %d)", (int)gameState.mouse.pos.y / TILE, (int)gameState.mouse.pos.x / TILE);
  Vector2 textSize = MeasureTextEx(GetFontDefault(), msg, fontSize, 1);
  Vector2 size = (Vector2){textSize.x + 1 * DEFAULT_UI_PADDING, textSize.y + 1 * DEFAULT_UI_PADDING};
  Vector2 mousePos = GetMousePosition();
  Vector2 pos = (Vector2){
      .x = mousePos.x - size.x,
      .y = mousePos.y - size.y,
  };
  DrawRectangle(pos.x, pos.y, size.x, size.y, RED);
  DrawText(msg, pos.x + DEFAULT_UI_PADDING / 2.0f, pos.y + DEFAULT_UI_PADDING / 2.0f, fontSize, WHITE);
}

void updateState() {
  computeMouseState();
  computeFloatingMenuKeys();

  if (currentFloatingMenuOpen.type == FLOATING_MENU_TYPE_PAUSE) {
    return;
  }

  float dt = GetFrameTime();

  computeCommands();
  computeSpawnEnemy(dt);
  computeLevelState(dt);
  computeWaveState(dt);
  computeBullet(dt);

  computeEnemiesMovement(dt);
  computeTowerAngle();
  computeNotificationDuration(dt);
}

void draw() {
  drawBakedScene();
  FOREACH_ACTIVE(enemy, enemies) drawEnemy(enemy);
  FOREACH_ACTIVE(bullet, bullets) drawBullet(bullet);
  FOREACH_ACTIVE(tower, towers) drawTower(tower);
  drawNotifications();

  drawLevelTransition();

  drawHud();

  drawPlacement();
  drawFloatingMenu();

#ifdef DEBUG_ENABLED
  drawDebugGrid();
#endif
}

void initState() {
  gameState.level.number = 1;
  gameState.level.waitingNext = false;
  gameState.level.timer = -1;
  gameState.level.sceneLoaded = true;

  gameState.wave.number = 0;
  gameState.wave.waitingNext = true;
  gameState.wave.timer = -1;

  buildPathMatrix();
  buildPath();
  bakeScene();
}

int main(void) {
#ifdef DEBUG_ENABLED
  SetTraceLogLevel(LOG_DEBUG);
#endif

  unsigned int seed = (unsigned int)time(NULL);
#ifdef DEBUG_ENABLED
  seed = 1234567890;
#endif

  TraceLog(LOG_INFO, "seed: %u", seed);

  InitWindow(TILE * GRID_COLS, TILE * GRID_ROWS, "tower defense");
  SetRandomSeed(seed);

  SetTargetFPS(60);
  SetExitKey(KEY_BACKSPACE);

  loadAssets();
  initState();

  while (!WindowShouldClose()) {
    updateState();

    BeginDrawing();
    ClearBackground(RAYWHITE);
    draw();

    EndDrawing();
  }

  unloadAssets();

  CloseWindow();

  return 0;
}
