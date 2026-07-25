#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "raylib.h"
#include "raymath.h"

#define TILE 80
#define GRID_COLS 20
#define GRID_ROWS 15

#define ENEMY_SPAWN_DELAY_IN_SECONDS 1
#define NEXT_WAVE_SPAWN_DELAY_IN_SECONDS 5
#define ENEMY_PER_WAVE 5

#define COUNT_OF(a) (int)(sizeof(a) / sizeof((a)[0]))

// ================================================== SHARED STATE ==================================================

typedef struct {
  float speed;
  int wave;
  float enemySpawnTimer;
  float nextWaveSpawnTimer;
  bool waitingNextWave;
  int level;
} GameState;

GameState gameState = {
    .speed = 200.0f,
    .wave = 1,
    .enemySpawnTimer = 0,
    .nextWaveSpawnTimer = -1,
    .waitingNextWave = false,
};

// ================================================== ENTITY_WRAPPER ==================================================

typedef enum { ENTITY_WRAPPER_TYPE_TOWER, ENTITY_WRAPPER_TYPE_ENEMY, ENTITY_WRAPPER_TYPE_BULLET } EntityWrapperType;

typedef struct {
  EntityWrapperType type;
  unsigned int id;
  int arrayIndex;
} EntityWrapper;

// ================================================== ENEMY ==================================================

typedef enum { ENEMY_TYPE_NORMAL, ENEMY_TYPE_RUNNER, ENEMY_TYPE_TANK, ENEMY_TYPE_BOSS, ENEMY_TYPE_COUNT } EnemyType;
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
  int lifePoints;
  EnemyType type;
  float angle;
} Enemy;

Enemy enemies[64];
int enemyCount = 0;

// ================================================== TOWER ==================================================

typedef enum { TOWER_TYPE_NORMAL, TOWER_TYPE_DOUBLE, TOWER_TYPE_COUNT } TowerType;
typedef struct {
  bool active;
  unsigned int entityId;

  Vector2 pos;
  Vector2 size;
  Color color;
  float range;
  float fireRate;
  float bulletCooldown;
  TowerType type;
  float angle;
} Tower;
Tower towers[64];

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

typedef struct {
  int lifePoints;
  int gold;
} Player;

Player player = {
    .lifePoints = 1000, //
    .gold = 1000        //
};

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
    if (entityWrapper.arrayIndex < 0 || entityWrapper.arrayIndex >= COUNT_OF(enemies)) {
      return NULL;
    }

    Enemy *enemy = &enemies[entityWrapper.arrayIndex];
    if (enemy->active == true && enemy->entityId == entityWrapper.id) {
      return enemy;
    }
    return NULL;
  }
  case ENTITY_WRAPPER_TYPE_TOWER: {
    if (entityWrapper.arrayIndex < 0 || entityWrapper.arrayIndex >= COUNT_OF(towers)) {
      return NULL;
    }

    Tower *tower = &towers[entityWrapper.arrayIndex];
    if (tower->active == true && tower->entityId == entityWrapper.id) {
      return tower;
    }
    return NULL;
  }
  case ENTITY_WRAPPER_TYPE_BULLET: {
    if (entityWrapper.arrayIndex < 0 || entityWrapper.arrayIndex >= COUNT_OF(bullets)) {
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

#define SPAWN(list) spawnEntity((list), COUNT_OF(list), sizeof((list)[0]))

void *spawnEntity(void *list, int count, size_t stride) {
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
  } while (pathCount < COUNT_OF(path));
}

void drawOnTile(int row, int col, int textureIndex) {
  Texture2D texture = sceneTextures[textureIndex];
  Rectangle source = {0, 0, texture.width, texture.height};
  Vector2 pos = cellCenter(col, row);
  Rectangle dest = {pos.x, pos.y, TILE, TILE};
  Vector2 origin = (Vector2){TILE * 0.5f, TILE * 0.5f};
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
  for (int i = 0; i < COUNT_OF(enemies); i++) {
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

// ================================================== ENEMY ==================================================

void spawnEnemy() {
  if (gameState.waitingNextWave) {
    return;
  }

  Enemy *enemy = SPAWN(enemies);
  if (enemy == NULL) {
    return;
  }

  enemy->type = GetRandomValue(0, ENEMY_TYPE_COUNT - 2);
  enemy->pos = path[0];
  enemy->size = (Vector2){65, 65};
  enemy->color = RED;
  enemy->targetIndex = 1;
  enemy->lifePoints = enemyStats[enemy->type].healthPoints;

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
  for (int i = 0; i < COUNT_OF(enemies); i++) {
    Enemy *enemy = &enemies[i];
    if (!enemy->active) {
      continue;
    }

    Vector2 target = path[enemy->targetIndex];
    enemy->pos = Vector2MoveTowards(enemy->pos, target, (gameState.speed * enemyStats[enemy->type].speedMultiplier) * dt);
    if (Vector2Equals(enemy->pos, target)) {
      enemy->targetIndex++;
    }

    if (enemy->targetIndex == pathCount) {
      enemy->active = false;
      player.lifePoints -= enemyStats[enemy->type].damage;
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
  for (int i = 0; i < COUNT_OF(enemies); i++) {
    Enemy *enemy = &enemies[i];
    if (enemy->active) {
      drawEnemy(enemy);
    }
  }
}

// ================================================== TOWER ==================================================

bool retrieveTowerTarget(Tower *tower, EntityWrapper *out) {
  for (int i = 0; i < COUNT_OF(enemies); i++) {
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

void spawnTower(Vector2 pos) {
  int posX = pos.x / TILE;
  int posY = pos.y / TILE;

  if (posX < 0 || posX >= GRID_COLS) {
    return;
  }

  if (posY <= 1 || posY >= GRID_ROWS - 1) {
    return;
  }

  if (pathMatrix[posY][posX]) {
    return;
  }

  if (player.gold < 100) {
    return;
  }
  player.gold -= 100;

  Tower *tower = SPAWN(towers);
  if (tower == NULL) {
    return;
  }

  tower->pos = cellCenter(posX, posY);
  tower->size = (Vector2){65, 65};
  tower->color = BLUE;
  tower->range = TILE / 2.0f + TILE * 2;
  tower->fireRate = 1.0f / 2;
  tower->bulletCooldown = tower->fireRate;
  tower->type = TOWER_TYPE_NORMAL;

  TraceLog(LOG_DEBUG, "tower spawned (%f %f)", tower->pos.x, tower->pos.y);
}

void computeTowerAngle() {
  for (int i = 0; i < COUNT_OF(towers); i++) {
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
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    spawnTower(GetMousePosition());
  }
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
  for (int i = 0; i < COUNT_OF(towers); i++) {
    Tower *tower = &towers[i];
    if (tower->active) {
      drawTower(tower);
    }
  }
}

// ================================================== BULLET ==================================================

void spawnBullet(Tower *tower, EntityWrapper target) {
  Bullet *bullet = SPAWN(bullets);
  if (bullet == NULL) {
    return;
  }

  bullet->pos = tower->pos;
  bullet->size = 5.0f;
  bullet->color = BLACK;
  bullet->target = target;
}

void computeSpawnBullet(float dt) {
  for (int i = 0; i < COUNT_OF(towers); i++) {
    Tower *tower = &towers[i];
    if (!tower->active) {
      continue;
    }

    tower->bulletCooldown += dt;
    if (tower->bulletCooldown < tower->fireRate) {
      continue;
    }
    tower->bulletCooldown -= tower->fireRate;

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
  enemy->lifePoints -= 10;

  if (enemy->lifePoints <= 0) {
    enemy->active = false;
    player.gold += enemyStats[enemy->type].gold;
  }
}

void computeBulletsMovement(float dt) {
  for (int i = 0; i < COUNT_OF(bullets); i++) {
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
  for (int i = 0; i < COUNT_OF(bullets); i++) {
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

void drawDebugGrid() {
  for (int col = 0; col <= GRID_COLS; col++) {
    int x = col * TILE;
    DrawLineDashed((Vector2){x, 0}, (Vector2){x, GRID_ROWS * TILE}, 5, 5, LIGHTGRAY);
  }
  for (int row = 0; row <= GRID_ROWS; row++) {
    int y = row * TILE;
    DrawLineDashed((Vector2){0, y}, (Vector2){GRID_COLS * TILE, y}, 5, 5, LIGHTGRAY);
  }
  for (int i = 0; i < COUNT_OF(towers); i++) {
    Tower tower = towers[i];
    if (tower.active) {
      DrawCircleLinesV(tower.pos, tower.range, LIGHTGRAY);
    }
  }
}

float drawStat(float x, const char *fmt, ...) {
  char buf[64];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  Vector2 size = MeasureTextEx(font, buf, 16, 1);
  DrawTextEx(font, buf, (Vector2){x, (TILE - size.y) / 2}, 16, 1, RAYWHITE);
  return x + size.x + 15;
}

void drawHud() {
  int maxWidth = GetScreenWidth();
  DrawRectangle(0, 0, maxWidth, TILE, BROWN);

  float x = 10;
  x = drawStat(x, "Life: %d", player.lifePoints);
  x = drawStat(x, "Wave: %d", gameState.wave);
  x = drawStat(x, "Gold: %d", player.gold);
  if (waveEnded()) {
    if (gameState.nextWaveSpawnTimer > 0) {
      x = drawStat(x, "Next Wave In: %.0f", (NEXT_WAVE_SPAWN_DELAY_IN_SECONDS - gameState.nextWaveSpawnTimer));
    }
  } else {
    int remainingEnemies = ENEMY_PER_WAVE * gameState.wave - enemyCount + 1;
    x = drawStat(x, "Remaining Enemies To Spawn: %d", remainingEnemies);
  }

  int maxHeight = GetScreenHeight();
  DrawRectangle(0, maxHeight - TILE, maxWidth, TILE, BROWN);

#ifdef DEBUG_ENABLED
  drawDebugGrid();
#endif
}

// ================================================== PATH ==================================================

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
}

void unloadAssets() {
  UnloadFont(font);
  UnloadRenderTexture(sceneBaked);

  for (int i = 0; i < COUNT_OF(enemyTextures); i++) {
    UnloadTexture(enemyTextures[i]);
  }
  for (int i = 0; i < COUNT_OF(towerTextures); i++) {
    UnloadTexture(towerTextures[i]);
  }
  for (int i = 0; i < COUNT_OF(sceneTextures); i++) {
    UnloadTexture(sceneTextures[i]);
  }
}

// ================================================== MAIN ==================================================

void updateState() {
  float dt = GetFrameTime();

  computeSpawnEnemy(dt);
  computeSpawnWave(dt);
  computeBullet(dt);

  computeEnemiesMovement(dt);
  updateTowerState();

  if (gameState.waitingNextWave && waveEnded() && gameState.nextWaveSpawnTimer < 0) {
    gameState.nextWaveSpawnTimer = 0;
  }
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

  loadAssets();

  while (!WindowShouldClose()) {
    if (gameState.level == 0) {
      nextLevel();
    }

    updateState();

    BeginDrawing();
    ClearBackground(RAYWHITE);

    drawBakedScene();
    drawEnemies();
    drawBullets();
    drawTowers();

    drawHud();

    EndDrawing();
  }

  unloadAssets();

  CloseWindow();

  return 0;
}
