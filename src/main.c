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
} GameState;

GameState gameState = {
    .speed = 200.0f,
    .wave = 1,
    .enemySpawnTimer = 0,
    .nextWaveSpawnTimer = -1,
    .waitingNextWave = false,
};

Font font;

// ================================================== ENTITY_WRAPPER ==================================================

typedef enum { ENTITY_WRAPPER_TYPE_TOWER, ENTITY_WRAPPER_TYPE_ENEMY, ENTITY_WRAPPER_TYPE_BULLET } EntityWrapperType;

typedef struct {
  EntityWrapperType type;
  unsigned int id;
  int arrayIndex;
} EntityWrapper;

// ================================================== ENEMY ==================================================

typedef enum { ENEMY_NORMAL, ENEMY_RUNNER, ENEMY_TANK, ENEMY_BOSS, ENEMY_TYPE_COUNT } EnemyType;
typedef struct {
  int healthPoints;
  float speedMultiplier;
  int gold;
  int damage;
} EnemyStat;

EnemyStat enemyStats[] = {
    [ENEMY_NORMAL] = {100, 1, 20, 10},
    [ENEMY_RUNNER] = {50, 1.5, 20, 10},
    [ENEMY_TANK] = {200, 0.5, 20, 10},
    [ENEMY_BOSS] = {100, 1, 20, 10},
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
} Enemy;

Enemy enemies[64];
int enemyCount = 0;

// ================================================== TOWER ==================================================

typedef struct {
  bool active;
  unsigned int entityId;

  Vector2 pos;
  Vector2 size;
  Color color;
  float range;
  float fireRate;
  float bulletCooldown;
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

// ================================================== PATH ==================================================

int pathMatrix[GRID_ROWS][GRID_COLS] = {
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

typedef struct {
  bool active;
  unsigned int entityId;
} EntityBase;

void *spawnEntity(void *list, int count, size_t stride) {
  int slot = nextInactiveSlot(list, count, stride);
  if (slot < 0) {
    return NULL;
  }

  EntityBase *entity = (EntityBase *)((char *)list + slot * stride);
  entity->active = true;
  entity->entityId = nextEntityId();

  return entity;
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
    if (enemy->active == false) {
      continue;
    }

    if (enemy->targetIndex == pathCount) {
      enemy->active = false;
      player.lifePoints -= enemyStats[enemy->type].damage;
      continue;
    }

    Vector2 target = path[enemy->targetIndex];
    enemy->pos = Vector2MoveTowards(enemy->pos, target, (gameState.speed * enemyStats[enemy->type].speedMultiplier) * dt);
    if (Vector2Equals(enemy->pos, target)) {
      enemy->targetIndex++;
    }
  }
}

void drawEnemy(Enemy *enemy) {
  Rectangle rectangle = {enemy->pos.x, enemy->pos.y, enemy->size.x, enemy->size.y};
  DrawRectanglePro(rectangle, Vector2Scale(enemy->size, 0.5f), 0, enemy->color);
  DrawTextEx(font, TextFormat("%d", enemy->lifePoints), Vector2Add(enemy->pos, (Vector2){-24, -7.5}), 16, 1, BLACK);
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

void spawnTower(Vector2 pos) {
  int posX = pos.x / TILE;
  int posY = pos.y / TILE;

  if (posY <= 1 || posY >= GRID_ROWS - 1 || pathMatrix[posY][posX]) {
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

  TraceLog(LOG_DEBUG, "tower spawned (%f %f)", tower->pos.x, tower->pos.y);
}

void updateTowerState() {
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    spawnTower(GetMousePosition());
  }
}

void drawTower(Tower *tower) {
  Rectangle rectangle = {tower->pos.x, tower->pos.y, tower->size.x, tower->size.y};
  Vector2 origin = Vector2Scale(tower->size, 0.5f);
  DrawRectanglePro(rectangle, origin, 0, tower->color);
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

    for (int j = 0; j < COUNT_OF(enemies); j++) {
      Enemy *enemy = &enemies[j];
      if (!enemy->active) {
        continue;
      }

      float distance = Vector2Distance(tower->pos, enemy->pos);
      if (distance < tower->range) {
        EntityWrapper entityWrapper = {.type = ENTITY_WRAPPER_TYPE_ENEMY, .id = enemy->entityId, .arrayIndex = j};
        spawnBullet(tower, entityWrapper);
        break;
      }
    }
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

void drawScene() {
  for (int i = 0; i < pathCount - 1; i++) {
    Vector2 startPos = path[i];
    Vector2 endPos = path[i + 1];

    DrawLineV(startPos, endPos, RED);
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
  buildPath();

  font = LoadFontEx("assets/fonts/PressStart2P-Regular.ttf", 16, NULL, 0);

  while (!WindowShouldClose()) {
    updateState();

    BeginDrawing();
    ClearBackground(RAYWHITE);

    drawScene();
    drawEnemies();
    drawTowers();
    drawBullets();

    drawHud();

    EndDrawing();
  }

  UnloadFont(font);

  CloseWindow();

  return 0;
}
