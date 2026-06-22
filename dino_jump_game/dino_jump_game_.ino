#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── Pin definitions ───────────────────────────────────────────
#define TOUCH_PIN    4    // TTP223 SIG output
#define BUZZER_PIN   18   // Passive buzzer (optional)

// ── OLED ──────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ── Layout constants ──────────────────────────────────────────
#define GROUND_Y       54          // y of ground line

// ── Dino constants ────────────────────────────────────────────
#define DINO_X         10          // fixed X on screen
#define DINO_W         11
#define DINO_H         15
#define DINO_FLOOR    (GROUND_Y - DINO_H)   // y when standing

// ── Jump physics ──────────────────────────────────────────────
#define JUMP_VY_TAP   -7.8f        // normal tap jump velocity
#define JUMP_VY_HOLD  -10.0f       // held touch = higher jump
#define GRAVITY        0.55f
#define HOLD_MS        130         // ms threshold: tap vs hold

// ── Cactus constants ──────────────────────────────────────────
#define CACTUS_W       10
#define CACTUS_H       18
#define CACTUS_FLOOR  (GROUND_Y - CACTUS_H)

// ── Speed ─────────────────────────────────────────────────────
#define BASE_SPEED     2.2f
#define MAX_SPEED      6.5f
#define SPEED_STEP     0.4f        // added per 200 pts level
#define LVL_EVERY      200

// ── Frame rate ────────────────────────────────────────────────
#define FRAME_MS       28          // ~35 fps

// ═════════════════════════════════════════════════════════════
//   Game state
// ═════════════════════════════════════════════════════════════
enum GState { ST_START, ST_PLAY, ST_DEAD };
GState gState = ST_START;

// Dino
float dinoY, dinoVY;
bool  onGround;

// Touch
bool  tPrev = false, tCur = false;
unsigned long tPressAt  = 0;
bool  jumpQueued = false;
bool  doHighJump = false;

// Obstacle (single cactus at a time)
float  cacX;
bool   cacActive;
unsigned long cacSpawnAt;
unsigned long cacInterval;

// Decorations
float  cloudX;
float  dotX[8];

// Score / speed
unsigned long score     = 0;
unsigned long hiScore   = 0;
float         spd       = BASE_SPEED;
unsigned long lastFrame = 0;

// Leg animation toggle
bool legPhase = false;
unsigned long legTimer = 0;

// ═════════════════════════════════════════════════════════════
//   Sprite drawers
// ═════════════════════════════════════════════════════════════

/* Dino – 11x15 pixel art */
void drawDino(int x, int y) {
  // Head block
  display.fillRect(x + 4, y,      7, 6, WHITE);
  // Eye
  display.drawPixel(x + 9, y + 1, BLACK);
  // Body
  display.fillRect(x + 1, y + 5,  9, 7, WHITE);
  display.fillRect(x,     y + 7,  8, 4, WHITE);
  // Tail tip
  display.drawPixel(x,     y + 7,  WHITE);
  display.drawPixel(x,     y + 8,  WHITE);
  display.drawPixel(x + 1, y + 10, WHITE);
  // Mini arm
  display.drawPixel(x + 7, y + 5,  WHITE);
  display.drawPixel(x + 8, y + 5,  WHITE);

  // Legs
  if (onGround) {
    if (!legPhase) {
      display.fillRect(x + 2, y + 12, 3, 3, WHITE);  // front leg down
      display.fillRect(x + 6, y + 12, 3, 2, WHITE);  // back leg up
    } else {
      display.fillRect(x + 2, y + 12, 3, 2, WHITE);  // front leg up
      display.fillRect(x + 6, y + 12, 3, 3, WHITE);  // back leg down
    }
  } else {
    // tucked in air
    display.fillRect(x + 2, y + 12, 3, 2, WHITE);
    display.fillRect(x + 6, y + 12, 3, 2, WHITE);
  }
}

/* Cactus – 10x18 */
void drawCactus(int x, int y) {
  display.fillRect(x + 3, y + 5,  4, 13, WHITE);  // trunk
  display.fillRect(x,     y + 8,  4,  2, WHITE);  // left arm horiz
  display.fillRect(x,     y + 4,  2,  6, WHITE);  // left arm vert
  display.fillRect(x + 7, y + 10, 3,  2, WHITE);  // right arm horiz
  display.fillRect(x + 8, y + 6,  2,  6, WHITE);  // right arm vert
  display.drawPixel(x + 4, y,      WHITE);         // top spike L
  display.drawPixel(x + 5, y,      WHITE);         // top spike R
  display.drawPixel(x + 5, y + 1,  WHITE);
  display.drawPixel(x + 1, y + 3,  WHITE);         // left tip
  display.drawPixel(x + 8, y + 5,  WHITE);         // right tip
}

/* Simple cloud */
void drawCloud(int x, int y) {
  display.fillRoundRect(x,     y + 3, 18, 5, 2, WHITE);
  display.fillRoundRect(x + 3, y,     11, 7, 3, WHITE);
}

/* Score display – top right */
void drawScore() {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(72, 1);
  display.print(F("SC:"));
  display.print(score);
  display.setCursor(72, 11);
  display.print(F("HI:"));
  display.print(hiScore);
}

// ═════════════════════════════════════════════════════════════
//   Screen drawers
// ═════════════════════════════════════════════════════════════
void showStartScreen() {
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, WHITE);

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(12, 5);
  display.print(F("DINO JUMP"));

  display.drawLine(0, 26, 128, 26, WHITE);

  display.setTextSize(1);
  display.setCursor(8, 32);
  display.print(F("TAP  touch = start"));
  display.setCursor(8, 43);
  display.print(F("TAP  = normal jump"));
  display.setCursor(8, 54);
  display.print(F("HOLD = high  jump"));

  display.display();
}

void showDeadScreen() {
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, WHITE);

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(8, 4);
  display.print(F("GAME OVER"));

  display.drawLine(0, 26, 128, 26, WHITE);

  display.setTextSize(1);
  display.setCursor(10, 30);
  display.print(F("Score : "));
  display.print(score);

  display.setCursor(10, 41);
  display.print(F("Best  : "));
  display.print(hiScore);

  display.setCursor(10, 54);
  display.print(F("TAP to play again"));

  display.display();
}

// ═════════════════════════════════════════════════════════════
//   Game reset
// ═════════════════════════════════════════════════════════════
void resetGame() {
  dinoY    = DINO_FLOOR;
  dinoVY   = 0;
  onGround = true;

  cacActive   = false;
  cacX        = 136;
  cacInterval = 2000;
  cacSpawnAt  = millis();

  cloudX = 110;

  for (int i = 0; i < 8; i++) dotX[i] = i * 16;

  score      = 0;
  spd        = BASE_SPEED;
  lastFrame  = millis();
  legTimer   = millis();
  legPhase   = false;
  jumpQueued = false;
  doHighJump = false;
}

// ═════════════════════════════════════════════════════════════
//   Touch – non-blocking, tap vs hold detection
// ═════════════════════════════════════════════════════════════
void handleTouch() {
  tCur = digitalRead(TOUCH_PIN);

  if (tCur && !tPrev) {          // rising edge – finger just placed
    tPressAt   = millis();
    jumpQueued = true;
    doHighJump = false;
  }
  if (tCur && tPrev) {           // still held
    if (millis() - tPressAt > HOLD_MS) doHighJump = true;
  }
  // falling edge – nothing extra needed

  tPrev = tCur;
}

// ═════════════════════════════════════════════════════════════
//   Sounds (buzzer optional – safe if not connected)
// ═════════════════════════════════════════════════════════════
void sndJump() { tone(BUZZER_PIN, 900,  55); }
void sndDead() {
  tone(BUZZER_PIN, 280, 80);
  delay(110);
  tone(BUZZER_PIN, 180, 160);
}

// ═════════════════════════════════════════════════════════════
//   Collision – AABB, inset 2-3 px for fairness
// ═════════════════════════════════════════════════════════════
bool hitTest() {
  int dx1 = DINO_X + 3,               dy1 = (int)dinoY + 3;
  int dx2 = DINO_X + DINO_W - 2,      dy2 = (int)dinoY + DINO_H - 1;
  int cx1 = (int)cacX + 1,            cy1 = CACTUS_FLOOR + 2;
  int cx2 = (int)cacX + CACTUS_W - 1, cy2 = CACTUS_FLOOR + CACTUS_H;
  return (dx1 < cx2 && dx2 > cx1 && dy1 < cy2 && dy2 > cy1);
}

// ═════════════════════════════════════════════════════════════
//   Setup
// ═════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN,  INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  Wire.begin(21, 22);   // SDA = GPIO21,  SCL = GPIO22

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 OLED not found! Check wiring."));
    for (;;);
  }

  display.clearDisplay();
  display.display();
  showStartScreen();
}

// ═════════════════════════════════════════════════════════════
//   Main loop
// ═════════════════════════════════════════════════════════════
void loop() {
  handleTouch();
  unsigned long now = millis();

  // ── START screen ──────────────────────────────────────────
  if (gState == ST_START) {
    if (jumpQueued) { jumpQueued = false; resetGame(); gState = ST_PLAY; }
    return;
  }

  // ── GAME OVER screen ──────────────────────────────────────
  if (gState == ST_DEAD) {
    if (jumpQueued) { jumpQueued = false; resetGame(); gState = ST_PLAY; }
    return;
  }

  // ── PLAYING ───────────────────────────────────────────────
  if (now - lastFrame < FRAME_MS) return;   // frame-rate cap
  lastFrame = now;

  // 1. Apply jump
  if (jumpQueued && onGround) {
    jumpQueued = false;
    onGround   = false;
    dinoVY     = doHighJump ? JUMP_VY_HOLD : JUMP_VY_TAP;
    doHighJump = false;
    sndJump();
  }

  // 2. Gravity
  if (!onGround) {
    dinoVY += GRAVITY;
    dinoY  += dinoVY;
    if (dinoY >= DINO_FLOOR) { dinoY = DINO_FLOOR; dinoVY = 0; onGround = true; }
  }

  // 3. Leg animation toggle (every 150 ms while running)
  if (onGround && now - legTimer > 150) { legPhase = !legPhase; legTimer = now; }

  // 4. Spawn cactus
  if (!cacActive && now - cacSpawnAt >= cacInterval) {
    cacActive   = true;
    cacX        = 132;
    cacInterval = (unsigned long)max(850L, (long)random(1300, 2600) - (long)(score / 4));
    cacSpawnAt  = now;
  }

  // 5. Move cactus
  if (cacActive) { cacX -= spd; if (cacX < -CACTUS_W) cacActive = false; }

  // 6. Cloud (parallax)
  cloudX -= spd * 0.3f;
  if (cloudX < -22) cloudX = 132;

  // 7. Ground dots
  for (int i = 0; i < 8; i++) { dotX[i] -= spd * 0.65f; if (dotX[i] < 0) dotX[i] += 128; }

  // 8. Score + speed
  score++;
  if (score > hiScore) hiScore = score;
  spd = BASE_SPEED + (score / LVL_EVERY) * SPEED_STEP;
  if (spd > MAX_SPEED) spd = MAX_SPEED;

  // 9. Collision
  if (cacActive && hitTest()) {
    sndDead();
    gState = ST_DEAD;
    showDeadScreen();
    return;
  }

  // 10. Render frame
  display.clearDisplay();

  display.drawLine(0, GROUND_Y, 128, GROUND_Y, WHITE);   // ground

  for (int i = 0; i < 8; i++) {                          // ground texture
    int gx = (int)dotX[i];
    display.drawPixel(gx,     GROUND_Y + 2, WHITE);
    display.drawPixel(gx + 3, GROUND_Y + 4, WHITE);
    display.drawPixel(gx + 7, GROUND_Y + 3, WHITE);
  }

  drawCloud((int)cloudX, 6);                             // cloud
  drawDino(DINO_X, (int)dinoY);                          // dino
  if (cacActive) drawCactus((int)cacX, CACTUS_FLOOR);    // cactus
  drawScore();                                            // HUD

  display.display();
}
