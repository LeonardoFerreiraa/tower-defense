#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "raylib.h"
#include "raymath.h"

#define TILE 80
#define GRID_COLS 20
#define GRID_ROWS 15
#define TILE_AS_VECTOR2                                                                                                                                        \
  (Vector2) {                                                                                                                                                  \
    TILE, TILE                                                                                                                                                 \
  }

#define ENEMY_SPAWN_DELAY_IN_SECONDS 1
#define NEXT_WAVE_SPAWN_DELAY_IN_SECONDS 5
#define ENEMY_PER_WAVE 5
#define NOTIFICATION_DURATION 5

#define MYBROWN (Color){129, 89, 46, 255}
#define MYBROWN_DARK (Color){77, 53, 28, 255}

#define BUTTON_LABEL_BUY "Buy"

#define FLOATING_MENU_ITEMS_PADDING 10

#define ARRAY_LEN(a) (int)(sizeof(a) / sizeof((a)[0]))
#define MATH_MAX(a, b) ((a) > (b) ? (a) : (b))
#define MATH_MIN(a, b) ((a) < (b) ? (a) : (b))

// ================================================== SHARED STATE ==================================================

struct {
  float speed;
  int wave;
  float enemySpawnTimer;
  float nextWaveSpawnTimer;
  bool waitingNextWave;
  int level;
} gameState = {
    .speed = 200.0f,
    .wave = 1,
    .enemySpawnTimer = 0,
    .nextWaveSpawnTimer = -1,
    .waitingNextWave = false,
};

// ================================================== ENTITY_WRAPPER ==================================================

typedef enum {
  ENTITY_WRAPPER_TYPE_TOWER,
  ENTITY_WRAPPER_TYPE_ENEMY,
  ENTITY_WRAPPER_TYPE_BULLET,
} EntityWrapperType;

typedef struct {
  EntityWrapperType type;
  unsigned int id;
  int arrayIndex;
} EntityWrapper;

// ================================================== ENEMY ==================================================

typedef enum {
  ENEMY_TYPE_NORMAL,
  ENEMY_TYPE_RUNNER,
  ENEMY_TYPE_TANK,
  ENEMY_TYPE_BOSS,
  ENEMY_TYPE_COUNT,
} EnemyType;

typedef struct {
  int healthPoints;
  float speedMultiplier;
  int gold;
  int damage;
} EnemyStat;

EnemyStat enemyStats[] = {
    [ENEMY_TYPE_NORMAL] = {100, 1, 20, 10},
    [ENEMY_TYPE_RUNNER] = {50, 1.5, 20, 10},
    [ENEMY_TYPE_TANK] = {200, 0.5, 20, 10},
    [ENEMY_TYPE_BOSS] = {100, 1, 20, 10},
};

typedef struct {
  bool active;
  unsigned int entityId;

  Vector2 pos;
  Vector2 size;
  Color color;
  int targetIndex;
  EnemyType type;
  float angle;

  EnemyStat stats;
} Enemy;

Enemy enemies[64];
int enemyCount = 0;

// ================================================== TOWER ==================================================

typedef enum {
  TOWER_TYPE_NORMAL,
  TOWER_TYPE_DOUBLE,
  TOWER_TYPE_COUNT,
} TowerType;

typedef struct {
  bool active;
  unsigned int entityId;

  Vector2 pos;
  Vector2 size;
  TowerType type;

  float range;
  int fireRate;
  float damage;

  float bulletCooldown;
  float angle;
} Tower;
Tower towers[64];

typedef struct {
  int cost;
  float range;
  int fireRate;
  float damage;
} TowerStat;

TowerStat towerStats[] = {
    [TOWER_TYPE_NORMAL] = {100, TILE * 2.5f, 2, 20},
    [TOWER_TYPE_DOUBLE] = {200, TILE * 2.5f, 4, 15},
};

// ================================================== BULLET ==================================================

typedef struct {
  bool active;
  unsigned int entityId;

  Vector2 pos;
  float size;
  Color color;
  EntityWrapper target;
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
  SCENE_TEXTURE_CORNER_COUNT
} SceneAsset;

Texture2D sceneTextures[SCENE_TEXTURE_CORNER_COUNT];
Texture2D enemyTextures[ENEMY_TYPE_COUNT];
Texture2D towerTextures[TOWER_TYPE_COUNT];

Font font;

RenderTexture2D sceneBaked;

// ================================================== PATH ==================================================

int pathMatrix[GRID_ROWS][GRID_COLS];
Vector2 path[GRID_ROWS * GRID_COLS];
int pathCount = 0;

// ================================================== PLAYER ==================================================

struct {
  int lifePoints;
  int gold;
} player = {
    .lifePoints = 1000,
    .gold = 300,
};

// ================================================== FLOATING MENU ==================================================

typedef enum {
  FLOATING_MENU_TYPE_SHOPPING,
  FLOATING_MENU_TYPE_PAUSE,
  FLOATING_MENU_TYPE_GAMEOVER,
  FLOATING_MENU_TYPE_NONE,
} FloatingMenuType;

FloatingMenuType currentFloatingMenuOpen = FLOATING_MENU_TYPE_NONE;

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

// ================================================== COMMANDS ==================================================

typedef enum {
  COMMAND_TYPE_BUY_TOWER,
  COMMAND_TYPE_PLACE_TOWER,
} CommandType;

typedef struct {
  union {
    struct {
      TowerType towerType;
    } buyTower;
    struct {
      TowerType towerType;
      Vector2 pos;
    } placeTower;
  };
} CommandCtx;

typedef struct {
  bool active;
  unsigned int entityId;

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
  NOTIFICATION_TYPE_SUCCESS,
  NOTIFICATION_TYPE_FAILURE,
} NotificationType;

typedef struct {
  bool active;
  unsigned int entityId;

  NotificationType type;
  char *message;

  float duration;
} Notification;

Notification notifications[32];

// ================================================== UTILITY ==================================================

int nextInactiveSlot(void *arr, int count, size_t stride) {
  for (int i = 0; i < count; i++) {
    bool active = *(bool *)((char *)arr + i * stride);
    if (!active) {
      return i;
    }
  }

  return -1;
}

Vector2 cellCenter(int col, int row) {
  return (Vector2){col * TILE + TILE / 2.0f, row * TILE + TILE / 2.0f};
}

void *resolveEntity(EntityWrapper entityWrapper) {
  switch (entityWrapper.type) {

  case ENTITY_WRAPPER_TYPE_ENEMY: {
    if (entityWrapper.arrayIndex < 0 || entityWrapper.arrayIndex >= ARRAY_LEN(enemies)) {
      return NULL;
    }

    Enemy *enemy = &enemies[entityWrapper.arrayIndex];
    if (enemy->active == true && enemy->entityId == entityWrapper.id) {
      return enemy;
    }
    return NULL;
  }
  case ENTITY_WRAPPER_TYPE_TOWER: {
    if (entityWrapper.arrayIndex < 0 || entityWrapper.arrayIndex >= ARRAY_LEN(towers)) {
      return NULL;
    }

    Tower *tower = &towers[entityWrapper.arrayIndex];
    if (tower->active == true && tower->entityId == entityWrapper.id) {
      return tower;
    }
    return NULL;
  }
  case ENTITY_WRAPPER_TYPE_BULLET: {
    if (entityWrapper.arrayIndex < 0 || entityWrapper.arrayIndex >= ARRAY_LEN(bullets)) {
      return NULL;
    }

    Bullet *bullet = &bullets[entityWrapper.arrayIndex];
    if (bullet->active == true && bullet->entityId == entityWrapper.id) {
      return bullet;
    }
    return NULL;
  }
  }

  return NULL;
}

unsigned int nextEntityId() {
  static unsigned int seq = 0;
  return ++seq;
}

#define NEW_ENTITY(list) newEntity((list), ARRAY_LEN(list), sizeof((list)[0]))

void *newEntity(void *list, int count, size_t stride) {
  typedef struct {
    bool active;
    unsigned int entityId;
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

// ================================================== COMMANDS ==================================================

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
  for (int i = 0; i < ARRAY_LEN(commands); i++) {
    Command *command = &commands[i];
    if (!command->active) {
      continue;
    }

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
  for (int i = 0; i < ARRAY_LEN(notifications); i++) {
    Notification *notification = &notifications[i];
    if (!notification->active) {
      continue;
    }

    notification->duration -= dt;
    if (notification->duration <= 0) {
      notification->active = false;
    }
  }
}

void drawNotification(Notification *notification) {
  // TODO improve
  DrawRectangle(100, 100, 100, 50, RED);
}

void drawNotifications() {
  for (int i = 0; i < ARRAY_LEN(notifications); i++) {
    Notification *notification = &notifications[i];
    if (!notification->active) {
      continue;
    }
    drawNotification(notification);
  }
}

// ================================================== SCENE ==================================================

void buildPathMatrix() {
  int fixed[GRID_ROWS][GRID_COLS] = {
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}, //
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}, //
      {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}, //
      {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //
      {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}, //
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}, //
      {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}, //
      {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //
      {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //
      {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //
      {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, //
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  };

  for (int row = 0; row < GRID_ROWS; row++) {
    for (int col = 0; col < GRID_COLS; col++) {
      pathMatrix[row][col] = fixed[row][col];
    }
  }
}

void buildPath() {
  int dCol[] = {0, 0, +1, -1};
  int dRow[] = {+1, -1, 0, 0};

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
  Texture2D texture = sceneTextures[textureIndex];
  Rectangle source = {0, 0, texture.width, texture.height};
  Vector2 pos = cellCenter(col, row);
  Rectangle dest = {pos.x, pos.y, TILE, TILE};
  Vector2 origin = Vector2Scale(TILE_AS_VECTOR2, 0.5f);
  DrawTexturePro(texture, source, dest, origin, 0, RAYWHITE);
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
  DrawTextureRec(sceneBaked.texture, (Rectangle){0, 0, sceneBaked.texture.width, -sceneBaked.texture.height}, (Vector2){0, 0}, RAYWHITE);
}

void nextLevel() {
  gameState.level++;
  buildPathMatrix();
  buildPath();
  bakeScene();
}

// ================================================== WAVE ==================================================

bool waveEnded() {
  for (int i = 0; i < ARRAY_LEN(enemies); i++) {
    if (enemies[i].active) {
      return false;
    }
  }

  return true;
}

void computeSpawnWave(float dt) {
  if (gameState.nextWaveSpawnTimer >= 0) {
    gameState.nextWaveSpawnTimer += dt;
    if (gameState.nextWaveSpawnTimer >= NEXT_WAVE_SPAWN_DELAY_IN_SECONDS) {
      gameState.nextWaveSpawnTimer = -1;
      gameState.wave++;
      gameState.waitingNextWave = false;
      enemyCount = 0;
    }
  }
}

void computeWaveState() {
  if (gameState.waitingNextWave && waveEnded() && gameState.nextWaveSpawnTimer < 0) {
    gameState.nextWaveSpawnTimer = 0;
  }
}

// ================================================== ENEMY ==================================================

EnemyStat computeEnemyStats(EnemyType enemyType) {
  EnemyStat defaultStats = enemyStats[enemyType];
  if (gameState.wave < 5) {
    return defaultStats;
  }

  float multiplier = MATH_MAX(gameState.wave / 5.0f, 1.0f);
  return (EnemyStat){
      .healthPoints = defaultStats.healthPoints * multiplier,
      .speedMultiplier = defaultStats.speedMultiplier,
      .gold = defaultStats.gold * multiplier,
      .damage = defaultStats.damage * multiplier,
  };
}

void spawnEnemy() {
  if (gameState.waitingNextWave) {
    return;
  }

  Enemy *enemy = NEW_ENTITY(enemies);
  if (enemy == NULL) {
    return;
  }

  enemy->type = GetRandomValue(0, ENEMY_TYPE_COUNT - 2);
  enemy->pos = path[0];
  enemy->size = Vector2Scale(TILE_AS_VECTOR2, 0.8f);
  enemy->color = RED;
  enemy->targetIndex = 1;
  enemy->stats = computeEnemyStats(enemy->type);

  if (++enemyCount > (ENEMY_PER_WAVE * gameState.wave)) {
    gameState.waitingNextWave = true;
  }
}

void computeSpawnEnemy(float dt) {
  gameState.enemySpawnTimer += dt;
  if (gameState.enemySpawnTimer >= ENEMY_SPAWN_DELAY_IN_SECONDS) {
    gameState.enemySpawnTimer -= ENEMY_SPAWN_DELAY_IN_SECONDS;
    spawnEnemy();
  }
}

void computeEnemiesMovement(float dt) {
  for (int i = 0; i < ARRAY_LEN(enemies); i++) {
    Enemy *enemy = &enemies[i];
    if (!enemy->active) {
      continue;
    }

    Vector2 target = path[enemy->targetIndex];
    enemy->pos = Vector2MoveTowards(enemy->pos, target, (gameState.speed * enemy->stats.speedMultiplier) * dt);
    if (Vector2Equals(enemy->pos, target)) {
      enemy->targetIndex++;
    }

    if (enemy->targetIndex == pathCount) {
      enemy->active = false;
      player.lifePoints -= enemy->stats.damage;
      continue;
    }

    Vector2 newTarget = path[enemy->targetIndex];
    Vector2 dir = Vector2Subtract(newTarget, enemy->pos);
    enemy->angle = atan2f(dir.y, dir.x) * RAD2DEG;
  }
}

void drawEnemy(Enemy *enemy) {
  Texture2D texture = enemyTextures[enemy->type];
  Rectangle source = {0, 0, texture.width, texture.height};
  Rectangle dest = {enemy->pos.x, enemy->pos.y, enemy->size.x, enemy->size.y};
  Vector2 origin = Vector2Scale(enemy->size, 0.5f);
  DrawTexturePro(texture, source, dest, origin, enemy->angle, RAYWHITE);
}

void drawEnemies() {
  for (int i = 0; i < ARRAY_LEN(enemies); i++) {
    Enemy *enemy = &enemies[i];
    if (enemy->active) {
      drawEnemy(enemy);
    }
  }
}

// ================================================== TOWER ==================================================

bool retrieveTowerTarget(Tower *tower, EntityWrapper *out) {
  for (int i = 0; i < ARRAY_LEN(enemies); i++) {
    Enemy *enemy = &enemies[i];
    if (!enemy->active) {
      continue;
    }

    float distance = Vector2Distance(tower->pos, enemy->pos);
    if (distance < tower->range) {
      out->type = ENTITY_WRAPPER_TYPE_ENEMY;
      out->id = enemy->entityId;
      out->arrayIndex = i;
      return true;
    }
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
  for (int i = 0; i < ARRAY_LEN(towers); i++) {
    Tower *tower = &towers[i];
    if (!tower->active) {
      continue;
    }

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

void updateTowerState() {
  computeTowerAngle();
}

void drawTower(Tower *tower) {
  Texture2D texture = towerTextures[tower->type];
  Rectangle source = {0, 0, texture.width, texture.height};
  Rectangle dest = {tower->pos.x, tower->pos.y, tower->size.x, tower->size.y};
  Vector2 origin = Vector2Scale(tower->size, 0.5f);
  DrawTexturePro(texture, source, dest, origin, tower->angle, RAYWHITE);
}

void drawTowers() {
  for (int i = 0; i < ARRAY_LEN(towers); i++) {
    Tower *tower = &towers[i];
    if (tower->active) {
      drawTower(tower);
    }
  }
}

bool validateTowerPlacement(PlacementCtx ctx, Vector2 pos) {
  if (ctx.type != PLACEMENT_CTX_TYPE_TOWER) {
    TraceLog(LOG_ERROR, "invalid type");
    return false;
  }

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

  for (int i = 0; i < ARRAY_LEN(towers); i++) {
    Tower tower = towers[i];
    if (!tower.active) {
      continue;
    }

    int towerPosX = tower.pos.x / TILE;
    int towerPosY = tower.pos.y / TILE;
    if (towerPosX == posX && towerPosY == posY) {
      return false;
    }
  }

  return true;
}

void drawTowerPlacementGhost(PlacementCtx ctx, Vector2 pos, bool canBePlaced) {
  if (ctx.type != PLACEMENT_CTX_TYPE_TOWER) {
    TraceLog(LOG_ERROR, "invalid type");
    return;
  }

  int posX = ((int)(pos.x / TILE)) * TILE + TILE / 2.0f;
  int posY = ((int)(pos.y / TILE)) * TILE + TILE / 2.0f;

  Texture2D texture = towerTextures[ctx.tower.towerType];
  Rectangle source = {0, 0, texture.width, texture.height};
  Rectangle dest = {posX, posY, TILE, TILE};
  Vector2 origin = Vector2Scale(TILE_AS_VECTOR2, 0.5f);
  Color color = canBePlaced ? Fade(GREEN, 0.7f) : Fade(RED, 0.7f);
  DrawTexturePro(texture, source, dest, origin, 0, color);
}

void computePlaceTowerCommand(CommandCtx ctx) {
  TowerType towerType = ctx.placeTower.towerType;
  Vector2 pos = ctx.placeTower.pos;

  TowerStat stat = towerStats[towerType];
  player.gold -= stat.cost;

  spawnTower(towerType, cellCenter(pos.x / TILE, pos.y / TILE));
}

void confirmTowerPlacement(PlacementCtx ctx, Vector2 pos) {
  if (ctx.type != PLACEMENT_CTX_TYPE_TOWER) {
    TraceLog(LOG_ERROR, "invalid type");
    return;
  }

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
  currentFloatingMenuOpen = FLOATING_MENU_TYPE_NONE;

  TowerStat towerStat = towerStats[towerType];
  if (player.gold < towerStat.cost) {
    pushNotification(NOTIFICATION_TYPE_FAILURE, "Insufficient Gold");
    return;
  }

  placement.active = true;
  placement.ctx = (PlacementCtx){.tower = {.towerType = towerType}};
  placement.validatePlacement = validateTowerPlacement;
  placement.confirmPlacement = confirmTowerPlacement;
  placement.drawPlacementGhost = drawTowerPlacementGhost;
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
}

void computeSpawnBullet(float dt) {
  for (int i = 0; i < ARRAY_LEN(towers); i++) {
    Tower *tower = &towers[i];
    if (!tower->active) {
      continue;
    }

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
  if (enemy == NULL) {
    bullet->active = false;
    return;
  }

  bullet->active = false;
  enemy->stats.healthPoints -= 10;

  if (enemy->stats.healthPoints <= 0) {
    enemy->active = false;
    player.gold += enemy->stats.gold;
  }
}

void computeBulletsMovement(float dt) {
  for (int i = 0; i < ARRAY_LEN(bullets); i++) {
    Bullet *bullet = &bullets[i];
    if (!bullet->active) {
      continue;
    }
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

void drawBullet(Bullet bullet) {
  DrawCircleV(bullet.pos, bullet.size, bullet.color);
}

void drawBullets() {
  for (int i = 0; i < ARRAY_LEN(bullets); i++) {
    Bullet bullet = bullets[i];
    if (bullet.active) {
      drawBullet(bullet);
    }
  }
}

void computeBullet(float dt) {
  computeSpawnBullet(dt);
  computeBulletsMovement(dt);
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
  x = drawStat(x, TextFormat("Wave: %d", gameState.wave));
  x = drawStat(x, TextFormat("Gold: %d", player.gold));
  if (waveEnded()) {
    if (gameState.nextWaveSpawnTimer > 0) {
      x = drawStat(x, TextFormat("Next Wave In: %.0f", (NEXT_WAVE_SPAWN_DELAY_IN_SECONDS - gameState.nextWaveSpawnTimer)));
    }
  } else {
    int remainingEnemies = ENEMY_PER_WAVE * gameState.wave - enemyCount + 1;
    x = drawStat(x, TextFormat("Remaining Enemies To Spawn: %d", remainingEnemies));
  }

  int maxHeight = GetScreenHeight();
  DrawRectangle(0, maxHeight - TILE, maxWidth, TILE, MYBROWN);
}

// ================================================== FLOATING MENU ==================================================

void computeFloatingMenuKeys() {
  if (IsKeyPressed(KEY_TAB) && currentFloatingMenuOpen == FLOATING_MENU_TYPE_NONE) {
    TraceLog(LOG_DEBUG, "tab pressed");
    placement.active = false;
    currentFloatingMenuOpen = FLOATING_MENU_TYPE_SHOPPING;
    return;
  }

  if (IsKeyPressed(KEY_ESCAPE) && currentFloatingMenuOpen == FLOATING_MENU_TYPE_NONE && !placement.active) {
    TraceLog(LOG_DEBUG, "esc pressed to pause");
    placement.active = false;
    currentFloatingMenuOpen = FLOATING_MENU_TYPE_PAUSE;
    return;
  }

  if (IsKeyPressed(KEY_ESCAPE)) {
    TraceLog(LOG_DEBUG, "esc pressed to close opened floating menu");
    placement.active = false;
    currentFloatingMenuOpen = FLOATING_MENU_TYPE_NONE;
    return;
  }
}

void drawFloatingMenuHeaderStrip(Vector2 floatingTileSize, Rectangle baseDest) {
  Vector2 origin = Vector2Scale(floatingTileSize, 0.5f);
  Rectangle source = {0, 0, floatingTileSize.x, floatingTileSize.y};

  Rectangle leftDest = {baseDest.x + 1.5 * TILE, baseDest.y, floatingTileSize.x, floatingTileSize.y};
  DrawTexturePro(floatingMenuAssets[FLOATING_MENU_ASSET_HEADER_LEFT], source, leftDest, origin, 0, RAYWHITE);

  Rectangle rightDest = {baseDest.x + baseDest.width - 1.5 * TILE, baseDest.y, floatingTileSize.x, floatingTileSize.y};
  DrawTexturePro(floatingMenuAssets[FLOATING_MENU_ASSET_HEADER_RIGHT], source, rightDest, origin, 0, RAYWHITE);

  float cur = leftDest.x + leftDest.width;
  while (cur < rightDest.x) {
    Rectangle dest = {cur, baseDest.y, MATH_MIN(floatingTileSize.x, rightDest.x - cur), floatingTileSize.y};
    DrawTexturePro(floatingMenuAssets[FLOATING_MENU_ASSET_HEADER_CENTER], source, dest, origin, 0, RAYWHITE);

    cur += floatingTileSize.x;
  };
}

Vector2 drawRawFloatingMenu(Vector2 sizeInTiles, char *headerText) {
  if (sizeInTiles.x < 5 || sizeInTiles.x > GRID_COLS || sizeInTiles.y < 0 || sizeInTiles.y > GRID_ROWS) {
    TraceLog(LOG_ERROR, "invalid size");
    return (Vector2){0, 0};
  }

  Vector2 floatingTileSize = {floatingMenuAssets[FLOATING_MENU_ASSET_BASE].width, floatingMenuAssets[FLOATING_MENU_ASSET_BASE].height};

  NPatchInfo nPatchInfo = {.source = {0, 0, floatingTileSize.x, floatingTileSize.y},
                           .left = floatingTileSize.x / 2,
                           .right = floatingTileSize.x / 2,
                           .top = floatingTileSize.y / 2,
                           .bottom = floatingTileSize.y / 2,
                           .layout = NPATCH_NINE_PATCH};

  Rectangle baseDest = {.x = (GRID_COLS - sizeInTiles.x) / 2 * TILE, //
                        .y = (GRID_ROWS - sizeInTiles.y) / 2 * TILE, //
                        .width = TILE * sizeInTiles.x,               //
                        .height = TILE * sizeInTiles.y};

  DrawTextureNPatch(floatingMenuAssets[FLOATING_MENU_ASSET_BASE], nPatchInfo, baseDest, (Vector2){0, 0}, 0, WHITE);

  drawFloatingMenuHeaderStrip(floatingTileSize, baseDest);

  Vector2 textSize = MeasureTextEx(font, headerText, font.baseSize, 1);
  DrawTextEx(font, headerText, (Vector2){baseDest.x + baseDest.width / 2 - textSize.x / 2, baseDest.y - textSize.y / 2}, font.baseSize, 1, RAYWHITE);

  return (Vector2){baseDest.x + TILE, baseDest.y + TILE * 0.75};
}

void drawPauseFloatingMenu() {
  Vector2 size = {6, 3};
  drawRawFloatingMenu(size, "PAUSE");
  // TODO: improve
}

Vector2 drawBuyTowerWidget(TowerType towerType, Vector2 size, Vector2 drawAt) {
  Texture2D towerTexture = towerTextures[towerType];
  TowerStat towerStat = towerStats[towerType];

  Rectangle rectangleWrapper = (Rectangle){drawAt.x, drawAt.y, (size.x - 2) * TILE, towerTexture.height};

  Vector2 buttonSize = {80, 50};
  Rectangle buttonRectangle = {.x = rectangleWrapper.x + rectangleWrapper.width - buttonSize.x - FLOATING_MENU_ITEMS_PADDING, //
                               .y = rectangleWrapper.y + rectangleWrapper.height - buttonSize.y - FLOATING_MENU_ITEMS_PADDING,
                               .width = buttonSize.x,
                               .height = buttonSize.y};
  bool mouseOverButton = CheckCollisionPointRec(GetMousePosition(), buttonRectangle);
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouseOverButton) {
    CommandCtx commandCtx = (CommandCtx){.buyTower = {.towerType = towerType}};
    pushCommand(COMMAND_TYPE_BUY_TOWER, computeBuyTowerCommand, commandCtx);
  }

  Texture buttonTexture = floatingMenuAssets[mouseOverButton ? FLOATING_MENU_ASSET_BUTTON_DEFAULT_HOVER : FLOATING_MENU_ASSET_BUTTON_DEFAULT];
  NPatchInfo nPatchInfo = {.source = {0, 0, buttonTexture.width, buttonTexture.height},
                           .left = buttonTexture.width / 2,
                           .right = buttonTexture.width / 2,
                           .top = buttonTexture.height / 2,
                           .bottom = buttonTexture.height / 2,
                           .layout = NPATCH_NINE_PATCH};

  Vector2 buttonTextSize = MeasureTextEx(font, BUTTON_LABEL_BUY, font.baseSize, 1);
  Vector2 buttonTextPos = {
      .x = buttonRectangle.x + buttonRectangle.width / 2 - buttonTextSize.x / 2,
      .y = buttonRectangle.y + buttonRectangle.height / 2 - buttonTextSize.y / 2,
  };

  const char *lines[] = {
      TextFormat("Cost: $%d", towerStat.cost),
      TextFormat("Range: %0.2f", towerStat.range),
      TextFormat("Fire Rate: %d/s", towerStat.fireRate),
      TextFormat("Damage: %0.2f", towerStat.damage),
  };
  Vector2 textPos = {
      .x = drawAt.x + towerTexture.width + FLOATING_MENU_ITEMS_PADDING,
      .y = drawAt.y + FLOATING_MENU_ITEMS_PADDING * 1.5,
  };
  for (int i = 0; i < ARRAY_LEN(lines); i++) {
    const char *msg = lines[i];
    Vector2 size = MeasureTextEx(font, msg, font.baseSize, 1);
    DrawTextEx(font, msg, textPos, font.baseSize, 1, MYBROWN_DARK);
    textPos.y += size.y + FLOATING_MENU_ITEMS_PADDING;
  }

  DrawRectangleRoundedLinesEx(rectangleWrapper, 0.1, 1, 2, MYBROWN);
  DrawTexture(towerTexture, drawAt.x, drawAt.y, RAYWHITE);
  DrawTextureNPatch(buttonTexture, nPatchInfo, buttonRectangle, (Vector2){0, 0}, 0, WHITE);
  DrawTextEx(font, BUTTON_LABEL_BUY, buttonTextPos, font.baseSize, 1, WHITE);

  return (Vector2){.x = drawAt.x, .y = drawAt.y + towerTexture.height + FLOATING_MENU_ITEMS_PADDING};
}

void drawShoppingFloatingMenu() {
  Vector2 size = {8, 11};
  Vector2 drawAt = drawRawFloatingMenu(size, "SHOPPING");

  for (int i = 0; i < TOWER_TYPE_COUNT; i++) {
    drawAt = drawBuyTowerWidget(i, size, drawAt);
  }
}

void drawFloatingMenu() {
  switch (currentFloatingMenuOpen) {
  case FLOATING_MENU_TYPE_SHOPPING:
    drawShoppingFloatingMenu();
    break;
  case FLOATING_MENU_TYPE_PAUSE:
    drawPauseFloatingMenu();
    break;
  case FLOATING_MENU_TYPE_GAMEOVER:
    break;
  case FLOATING_MENU_TYPE_NONE:
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
}

void updateState() {
  computeFloatingMenuKeys();

  if (currentFloatingMenuOpen == FLOATING_MENU_TYPE_PAUSE) {
    return;
  }

  float dt = GetFrameTime();

  computeCommands();
  computeSpawnEnemy(dt);
  computeSpawnWave(dt);
  computeBullet(dt);

  computeEnemiesMovement(dt);
  updateTowerState();
  computeWaveState();
  computeNotificationDuration(dt);
}

void draw() {
  drawBakedScene();
  drawEnemies();
  drawBullets();
  drawTowers();
  drawNotifications();

  drawHud();

  drawPlacement();
  drawFloatingMenu();

#ifdef DEBUG_ENABLED
  drawDebugGrid();
#endif
}

int main(void) {
#ifdef DEBUG_ENABLED
  SetTraceLogLevel(LOG_DEBUG);
#endif

  unsigned int seed = (unsigned int)time(NULL);
#ifdef DEBUG_ENABLED
  seed = 123456;
#endif

  TraceLog(LOG_INFO, "seed: %u", seed);
  SetRandomSeed(seed);

  InitWindow(TILE * GRID_COLS, TILE * GRID_ROWS, "tower defense");

  SetTargetFPS(60);
  SetExitKey(KEY_Q);

  loadAssets();

  while (!WindowShouldClose()) {
    if (gameState.level == 0) {
      nextLevel();
    }

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
