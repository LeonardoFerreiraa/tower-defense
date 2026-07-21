#include <stdarg.h>
#include <stdio.h>

#include "raylib.h"
#include "raymath.h"

#define SPAWN_AT_SECONDS 1
#define HEADER_HEIGHT 50
#define FOOTER_HEIGHT 50

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

Enemy enemies[3];
int enemyCount = 0;

Vector2 path[] = {
    {-30, HEADER_HEIGHT + 10 + 0},   //
    {750, HEADER_HEIGHT + 10 + 0},   // >
    {750, HEADER_HEIGHT + 10 + 50},  // \/
    {50, HEADER_HEIGHT + 10 + 50},   // <
    {50, HEADER_HEIGHT + 10 + 100},  // \/
    {770, HEADER_HEIGHT + 10 + 100}, // >
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

void drawScene() {
  for (int i = 0; i < COUNT_OF(path) - 1; i++) {
    Vector2 startPos = path[i];
    Vector2 endPos = path[i + 1];
    DrawLineV(startPos, endPos, GREEN);
  }
}

void drawEnemy(Enemy *enemy) {
  DrawRectangleV(enemy->pos, enemy->size, enemy->color);
  DrawTextEx(font,                                    //
             TextFormat("%d", enemy->lifePoints),     //
             Vector2Add(enemy->pos, (Vector2){5, 5}), //
             16,                                      //
             1,                                       //
             RAYWHITE);
}

void spawnEnemy() {
  if (enemyCount >= COUNT_OF(enemies)) {
    return;
  }

  Vector2 enemySize = {65, 65};

  Enemy *enemy = &enemies[enemyCount++];
  enemy->pos = path[0];
  enemy->size = enemySize;
  enemy->color = RED;
  enemy->targetIndex = 1;
  enemy->active = true;
  enemy->lifePoints = 100;
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

void drawHeader() {
  int maxWidth = GetScreenWidth();
  DrawRectangle(0, 0, maxWidth, HEADER_HEIGHT, BROWN);

  float x = 10;
  x = drawStat(x, "Life: %d |", player.lifePoints);
  x = drawStat(x, "Wave: %d |", wave);
  x = drawStat(x, "Gold: %d", player.gold);
}

void drawFooter() {
  int maxWidth = GetScreenWidth();
  int maxHeight = GetScreenHeight();
  DrawRectangle(0, maxHeight - FOOTER_HEIGHT, maxWidth, FOOTER_HEIGHT, BROWN);
}

void update() {
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

    if (enemy->targetIndex == COUNT_OF(path)) {
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
}

void drawEnemies() {
  for (int i = 0; i < COUNT_OF(enemies); i++) {
    Enemy *enemy = &enemies[i];
    if (enemy->active) {
      drawEnemy(enemy);
    }
  }
}

int main(void) {
  InitWindow(1600, 1200, "tower defense");
  SetTargetFPS(60);

  font = LoadFontEx("assets/fonts/PressStart2P-Regular.ttf", 16, NULL, 0);

  while (!WindowShouldClose()) {
    update();

    BeginDrawing();
    ClearBackground(RAYWHITE);

    drawEnemies();

    drawHeader();
    drawScene();
    drawFooter();

    EndDrawing();
  }

  UnloadFont(font);

  CloseWindow();

  return 0;
}
