#include <stdarg.h>
#include <stdio.h>

#include "raylib.h"
#include "raymath.h"

#define TILE 80
#define GRID_COLS 20
#define GRID_ROWS 15

#define SPAWN_AT_SECONDS 1

#define COUNT_OF(a) (int)(sizeof(a) / sizeof((a)[0]))

float speed = 200.0f;
int wave = 1;

float spawnTimer = 0;

typedef struct {
  Vector2 pos;
  Vector2 size;
  Color color;
  int targetIndex;
  bool active;
  int lifePoints;
} Enemy;

Enemy enemies[64];
int enemyCount = 0;

typedef struct {
  Vector2 pos;
  Vector2 size;
  Color color;
  bool active;
} Tower;
Tower towers[64];
int towerCount = 0;

Vector2 path[GRID_ROWS * GRID_COLS];
int pathCount = 0;

int pathMatrix[GRID_ROWS][GRID_COLS] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

typedef struct {
  int lifePoints;
  int gold;
} Player;

Player player = {
    .lifePoints = 1000, //
    .gold = 1000        //
};

Font font;

Vector2 cellCenter(int col, int row) {
  return (Vector2){col * TILE + TILE / 2.0f, row * TILE + TILE / 2.0f};
}

void drawScene() {
  for (int i = 0; i < pathCount - 1; i++) {
    Vector2 startPos = path[i];
    Vector2 endPos = path[i + 1];

    DrawLineV(startPos, endPos, RED);
  }
}

void drawEnemy(Enemy *enemy) {
  Rectangle rectangle = {enemy->pos.x, enemy->pos.y, enemy->size.x,
                         enemy->size.y};
  DrawRectanglePro(rectangle, Vector2Scale(enemy->size, 0.5f), 0, enemy->color);
  DrawTextEx(font,                                         //
             TextFormat("%d", enemy->lifePoints),          //
             Vector2Add(enemy->pos, (Vector2){-24, -7.5}), //
             16,                                           //
             1,                                            //
             BLACK);
}

void drawTower(Tower *tower) {
  Rectangle rectangle = {tower->pos.x, tower->pos.y, tower->size.x,
                         tower->size.y};
  DrawRectanglePro(rectangle, Vector2Scale(tower->size, 0.5f), 0, tower->color);
}

void spawnEnemy() {
  if (enemyCount >= COUNT_OF(enemies)) {
    return;
  }

  Enemy *enemy = &enemies[enemyCount++];
  enemy->pos = path[0];
  enemy->size = (Vector2){65, 65};
  enemy->color = RED;
  enemy->targetIndex = 1;
  enemy->active = true;
  enemy->lifePoints = 100;
}

void spawnTower(Vector2 pos) {
  if (towerCount >= COUNT_OF(towers)) {
    return;
  }

  int posX = pos.x / TILE;
  int posY = pos.y / TILE;

  printf("posY:%d\n", posY);
  if (posY <= 1 || posY >= GRID_ROWS - 1 || pathMatrix[posY][posX]) {
    return;
  }

  Tower *tower = &towers[towerCount++];
  tower->pos = cellCenter(posX, posY);
  tower->size = (Vector2){65, 65};
  tower->color = BLUE;
  tower->active = true;
}

float drawStat(float x, const char *fmt, ...) {
  char buf[64];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  Vector2 size = MeasureTextEx(font, buf, 16, 1);
  DrawTextEx(font, buf, (Vector2){x, 15}, 16, 1, RAYWHITE);
  return x + size.x + 15;
}

void drawHud() {
  int maxWidth = GetScreenWidth();
  DrawRectangle(0, 0, maxWidth, TILE, BROWN);

  float x = 10;
  x = drawStat(x, "Life: %d |", player.lifePoints);
  x = drawStat(x, "Wave: %d |", wave);
  x = drawStat(x, "Gold: %d", player.gold);

  int maxHeight = GetScreenHeight();
  DrawRectangle(0, maxHeight - TILE, maxWidth, TILE, BROWN);
}

void updateState() {
  float dt = GetFrameTime();

  spawnTimer += dt;
  if (spawnTimer >= SPAWN_AT_SECONDS) {
    spawnTimer -= SPAWN_AT_SECONDS;
    spawnEnemy();
  }

  for (int i = 0; i < COUNT_OF(enemies); i++) {
    Enemy *enemy = &enemies[i];
    if (enemy->active == false) {
      continue;
    }

    if (enemy->targetIndex == pathCount) {
      enemy->active = false;
      player.lifePoints -= enemy->lifePoints;
      continue;
    }

    Vector2 target = path[enemy->targetIndex];
    enemy->pos = Vector2MoveTowards(enemy->pos, target, speed * dt);
    if (Vector2Equals(enemy->pos, target)) {
      enemy->targetIndex++;
    }
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    spawnTower(GetMousePosition());
  }
}

void drawEnemies() {
  for (int i = 0; i < COUNT_OF(enemies); i++) {
    Enemy *enemy = &enemies[i];
    if (enemy->active) {
      drawEnemy(enemy);
    }
  }
}

void drawTowers() {
  for (int i = 0; i < COUNT_OF(towers); i++) {
    Tower *tower = &towers[i];
    if (tower->active) {
      drawTower(tower);
    }
  }
}

void drawDebugGrid() {
  for (int col = 0; col <= GRID_COLS; col++) {
    int x = col * TILE;
    DrawLineDashed((Vector2){x, 0}, (Vector2){x, GRID_ROWS * TILE}, 5, 5,
                   LIGHTGRAY);
  }
  for (int row = 0; row <= GRID_ROWS; row++) {
    int y = row * TILE;
    DrawLineDashed((Vector2){0, y}, (Vector2){GRID_COLS * TILE, y}, 5, 5,
                   LIGHTGRAY);
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

int main(void) {
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

    drawHud();
    drawDebugGrid();

    EndDrawing();
  }

  UnloadFont(font);

  CloseWindow();

  return 0;
}
