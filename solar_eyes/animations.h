#pragma once
#include "display.h"

// The v1 eyes: two filled rounded rectangles with arched brows and no pupils. They
// look around by changing shape. When the face turns to one side, the near eye gets a
// little bigger, the far eye a little smaller, and the two slide closer together.
// Every change eases in over a few frames instead of snapping.
//
// The button picks a mood, which tweaks the resting look and changes which
// expressions come up. Shaking overrides the mood: the eyes go wide, then dizzy.

enum class Expr : uint8_t {
  Idle,
  Blink,
  LookSide,
  LookUp,
  LookDown,
  Happy,
  Wink,
  Sleepy
};

enum class Mood : uint8_t {
  Normal,
  Happy,
  Sleepy,
  Angry,
  Sad
};

constexpr uint8_t MOOD_COUNT = 5;
const char *const MOOD_NAMES[MOOD_COUNT] = {"normal", "happy", "sleepy", "angry", "sad"};

enum class Phase : uint8_t {
  Awake,
  Shaken,
  Dizzy,
  Dozing,
  Asleep
};

// Rows 0-15 are the yellow glass. Leaving them off keeps the lit pixels blue.
static const int BLUE_TOP = 16;
static const int LEFT_X = 34;
static const int RIGHT_X = 96;
static const int EYE_Y = 44;
static const int BROW_Y = 18;

Expr expr = Expr::Idle;
Mood mood = Mood::Normal;
Phase phase = Phase::Awake;
int8_t glanceDir = 1;
bool winkLeft = true;
uint32_t exprStart = 0;
uint32_t exprEnd = 0;
uint32_t dizzyEnd = 0;

void enterExpr(Expr next, uint32_t now, uint32_t durationMs) {
  expr = next;
  exprStart = now;
  exprEnd = now + durationMs;
}

struct Pose {
  int dxL;
  int dxR;
  int dyL;
  int dyR;
  int wL;
  int wR;
  int hL;
  int hR;
  int browL;
  int browR;
  int archL;
  int archR;
  int tiltL;   // drops the inner end of the brow; negative raises it
  int tiltR;
  int cheekL;  // rows of the eye hidden under a curve from below
  int cheekR;
};

const Pose REST = {0, 0, 0, 0, 26, 26, 34, 34, 0, 0, 4, 4, 0, 0, 0, 0};
Pose shown = REST;

void approach(int &value, int target, int step) {
  if (value < target) {
    int next = value + step;
    value = next > target ? target : next;
  } else if (value > target) {
    int next = value - step;
    value = next < target ? target : next;
  }
}

// What comes next in each mood. Weights add up to 100, and the first entry is the
// resting expression. Normal is the v1 mix.
struct Step {
  Expr expr;
  uint8_t weight;
  uint16_t ms;
  uint16_t randomMs;
};

const Step NORMAL_STEPS[] = {
    {Expr::Idle, 18, 420, 280},
    {Expr::Blink, 16, 170, 0},
    {Expr::LookSide, 24, 480, 280},
    {Expr::LookUp, 14, 420, 0},
    {Expr::LookDown, 12, 420, 0},
    {Expr::Wink, 9, 260, 0},
    {Expr::Sleepy, 7, 700, 0},
};

const Step HAPPY_STEPS[] = {
    {Expr::Happy, 34, 500, 400},
    {Expr::Blink, 14, 170, 0},
    {Expr::LookSide, 22, 480, 280},
    {Expr::LookUp, 16, 420, 0},
    {Expr::Wink, 14, 260, 0},
};

const Step SLEEPY_STEPS[] = {
    {Expr::Sleepy, 40, 900, 600},
    {Expr::Blink, 30, 700, 300},
    {Expr::LookDown, 20, 700, 0},
    {Expr::LookSide, 10, 800, 0},
};

const Step ANGRY_STEPS[] = {
    {Expr::Idle, 40, 600, 400},
    {Expr::Blink, 12, 170, 0},
    {Expr::LookSide, 36, 600, 300},
    {Expr::LookDown, 12, 420, 0},
};

const Step SAD_STEPS[] = {
    {Expr::Idle, 34, 700, 500},
    {Expr::Blink, 14, 220, 0},
    {Expr::LookDown, 36, 900, 400},
    {Expr::LookSide, 16, 700, 300},
};

struct StepList {
  const Step *steps;
  uint8_t count;
};

template <size_t N>
constexpr StepList stepsOf(const Step (&steps)[N]) {
  return {steps, N};
}

const StepList MOOD_STEPS[MOOD_COUNT] = {
    stepsOf(NORMAL_STEPS), stepsOf(HAPPY_STEPS), stepsOf(SLEEPY_STEPS),
    stepsOf(ANGRY_STEPS), stepsOf(SAD_STEPS)};

// A random number from 0 to n - 1.
uint32_t randomBelow(uint32_t n) {
  return (uint32_t)random((long)n);
}

void startStep(const Step &step, uint32_t now) {
  if (step.expr == Expr::LookSide) {
    glanceDir = randomBelow(2) ? 1 : -1;
  } else if (step.expr == Expr::Wink) {
    winkLeft = randomBelow(2);
  }
  uint32_t ms = step.ms;
  if (step.randomMs) {
    ms += randomBelow(step.randomMs);
  }
  enterExpr(step.expr, now, ms);
}

void advanceExpr(uint32_t now) {
  const StepList &list = MOOD_STEPS[(uint8_t)mood];
  uint32_t roll = randomBelow(100);
  for (uint8_t i = 0; i < list.count; i++) {
    if (roll < list.steps[i].weight) {
      startStep(list.steps[i], now);
      return;
    }
    roll -= list.steps[i].weight;
  }
  startStep(list.steps[0], now);
}

void drawEyeShape(int cx, int cy, int w, int h, int cheek) {
  if (w < 10) {
    w = 10;
  }
  if (h < 3) {
    h = 3;
  }
  int y = cy - h / 2;
  if (y < BLUE_TOP) {
    h -= BLUE_TOP - y;
    y = BLUE_TOP;
  }
  int radius = 5;
  if (radius > w / 2) {
    radius = w / 2;
  }
  if (radius > h / 2) {
    radius = h / 2;
  }
  u8g2.setDrawColor(1);
  u8g2.drawRBox(cx - w / 2, y, w, h, radius);
  if (cheek > 0) {
    // A dark disc pushed up from below turns the eye into an arch.
    int r = w * 2 / 3;
    u8g2.setDrawColor(0);
    u8g2.drawDisc(cx, y + h - cheek + r, r);
    u8g2.setDrawColor(1);
  }
}

// innerRight is true for the left eye, whose inner end faces the middle of the face.
void drawBrow(int cx, int y, int w, int arch, int tilt, bool innerRight) {
  int x0 = cx - w / 2;
  int x1 = cx + w / 2;
  int y0 = innerRight ? y - tilt : y + tilt;
  int y1 = innerRight ? y + tilt : y - tilt;
  int yMid = y - arch;
  if (y0 < BLUE_TOP) {
    y0 = BLUE_TOP;
  }
  if (y1 < BLUE_TOP) {
    y1 = BLUE_TOP;
  }
  if (yMid < BLUE_TOP) {
    yMid = BLUE_TOP;
  }
  u8g2.setDrawColor(1);
  for (int t = 0; t < 2; t++) {
    u8g2.drawLine(x0, y0 + t, cx, yMid + t);
    u8g2.drawLine(cx, yMid + t, x1, y1 + t);
  }
}

void keepBlueOnly() {
  memset(u8g2.getBufferPtr(), 0, 128 * 2);
}

Pose exprPose(Expr e) {
  Pose pose = REST;
  switch (e) {
    case Expr::Idle:
      break;
    case Expr::Blink:
      pose.hL = 4;
      pose.hR = 4;
      pose.browL = 2;
      pose.browR = 2;
      pose.archL = 2;
      pose.archR = 2;
      break;
    case Expr::LookSide:
      if (glanceDir < 0) {
        pose.dxL = -2;
        pose.dxR = -8;
        pose.wL = 28;
        pose.wR = 23;
        pose.hL = 36;
        pose.hR = 31;
        pose.browL = -1;
        pose.browR = 0;
        pose.archL = 5;
        pose.archR = 3;
      } else {
        pose.dxL = 8;
        pose.dxR = 2;
        pose.wL = 23;
        pose.wR = 28;
        pose.hL = 31;
        pose.hR = 36;
        pose.browL = 0;
        pose.browR = -1;
        pose.archL = 3;
        pose.archR = 5;
      }
      break;
    case Expr::LookUp:
      pose.dyL = -2;
      pose.dyR = -2;
      pose.hL = 36;
      pose.hR = 36;
      pose.browL = -2;
      pose.browR = -2;
      pose.archL = 5;
      pose.archR = 5;
      break;
    case Expr::LookDown:
      pose.dyL = 3;
      pose.dyR = 3;
      pose.hL = 26;
      pose.hR = 26;
      pose.browL = 2;
      pose.browR = 2;
      pose.archL = 2;
      pose.archR = 2;
      break;
    case Expr::Happy:
      pose.wL = 28;
      pose.wR = 28;
      pose.hL = 32;
      pose.hR = 32;
      pose.browL = -2;
      pose.browR = -2;
      pose.archL = 5;
      pose.archR = 5;
      break;
    case Expr::Wink:
      if (winkLeft) {
        pose.dxL = 2;
        pose.hL = 4;
        pose.browL = 1;
        pose.archL = 2;
      } else {
        pose.dxR = -2;
        pose.hR = 4;
        pose.browR = 1;
        pose.archR = 2;
      }
      break;
    case Expr::Sleepy:
      pose.dyL = 2;
      pose.dyR = 2;
      pose.hL = 22;
      pose.hR = 22;
      pose.browL = 2;
      pose.browR = 2;
      pose.archL = 2;
      pose.archR = 2;
      break;
  }
  return pose;
}

// Shrinks an eye, but never past a blink.
void squint(int &h, int by) {
  h -= by;
  if (h < 4) {
    h = 4;
  }
}

void applyMood(Pose &pose) {
  switch (mood) {
    case Mood::Normal:
      break;
    case Mood::Happy:
      // Raised brows would flatten against the yellow rows, so they sit one row lower
      // than usual, where the arch has room to show.
      pose.cheekL = pose.hL > 8 ? pose.hL / 2 : 0;
      pose.cheekR = pose.hR > 8 ? pose.hR / 2 : 0;
      pose.browL = 1;
      pose.browR = 1;
      pose.archL = 3;
      pose.archR = 3;
      break;
    case Mood::Sleepy:
      if (pose.hL > 18) {
        pose.hL = 18;
      }
      if (pose.hR > 18) {
        pose.hR = 18;
      }
      pose.dyL += 3;
      pose.dyR += 3;
      pose.browL += 3;
      pose.browR += 3;
      pose.archL = 2;
      pose.archR = 2;
      break;
    case Mood::Angry:
      squint(pose.hL, 8);
      squint(pose.hR, 8);
      pose.browL += 4;
      pose.browR += 4;
      pose.archL = 0;
      pose.archR = 0;
      pose.tiltL = 5;
      pose.tiltR = 5;
      break;
    case Mood::Sad:
      squint(pose.hL, 6);
      squint(pose.hR, 6);
      pose.dyL += 3;
      pose.dyR += 3;
      pose.browL += 1;
      pose.browR += 1;
      pose.archL = 1;
      pose.archR = 1;
      pose.tiltL = -4;
      pose.tiltR = -4;
      break;
  }
}

// Each eye bounces around inside the screen when the keychain is shaken, like a weight on
// a spring: it lags behind the case, springs back, and rebounds off the edges. The two
// springs differ a little so the eyes do not move in step.
constexpr float BOUNCE_PUSH = 4000.0f;  // px/s^2 for each g of case acceleration
constexpr float BOUNCE_X = 14.0f;       // how far an eye can travel sideways
constexpr float BOUNCE_UP = 9.0f;       // and up, before the yellow rows
constexpr float BOUNCE_DOWN = 3.0f;     // and down, before the bottom edge

struct Bounce {
  float x;
  float y;
  float vx;
  float vy;
  float spring;
  float damping;
};

Bounce bounceL = {0, 0, 0, 0, 230.0f, 6.5f};
Bounce bounceR = {0, 0, 0, 0, 330.0f, 8.0f};
float pushX = 0;
float pushY = 0;
float tiltX = 0;
float tiltY = 0;
uint32_t lastPhysics = 0;

// The latest accelerometer reading: the tilt for the gaze, and the case's acceleration
// in g for the bounce.
void faceMotion(float tx, float ty, float px, float py) {
  tiltX = tx;
  tiltY = ty;
  pushX = px;
  pushY = py;
}

void stepBounce(Bounce &b, float dt) {
  // Screen y grows downward, so a push toward the top sends the eye down.
  float ax = -pushX * BOUNCE_PUSH - b.spring * b.x - b.damping * b.vx;
  float ay = pushY * BOUNCE_PUSH - b.spring * b.y - b.damping * b.vy;
  b.vx += ax * dt;
  b.vy += ay * dt;
  b.x += b.vx * dt;
  b.y += b.vy * dt;
  if (fabsf(b.x) > BOUNCE_X) {
    b.x = b.x > 0 ? BOUNCE_X : -BOUNCE_X;
    b.vx = -0.6f * b.vx;
  }
  if (b.y > BOUNCE_DOWN) {
    b.y = BOUNCE_DOWN;
    b.vy = -0.6f * b.vy;
  } else if (b.y < -BOUNCE_UP) {
    b.y = -BOUNCE_UP;
    b.vy = -0.6f * b.vy;
  }
}

void stepPhysics(uint32_t now) {
  float dt = (now - lastPhysics) / 1000.0f;
  lastPhysics = now;
  if (dt > 0.05f) {
    dt = 0.05f;
  }
  for (int i = 0; i < 2; i++) {
    stepBounce(bounceL, dt / 2);
    stepBounce(bounceR, dt / 2);
  }
}

int scaled(float amount, int full) {
  return (int)lroundf(amount * full);
}

// Looks the opposite way to the tilt, scaled from the v1 glances: the eye on the side
// being looked at grows, the other shrinks, and both slide over.
void applyTilt(Pose &pose) {
  bool openL = pose.hL > 8;
  bool openR = pose.hR > 8;
  if (tiltX != 0) {
    float s = fabsf(tiltX);
    bool right = tiltX > 0;
    pose.dxL += scaled(s, right ? 14 : -8);
    pose.dxR += scaled(s, right ? 8 : -14);
    pose.wL += scaled(s, right ? -3 : 2);
    pose.wR += scaled(s, right ? 2 : -3);
    if (openL) {
      pose.hL += scaled(s, right ? -3 : 2);
    }
    if (openR) {
      pose.hR += scaled(s, right ? 2 : -3);
    }
  }
  if (tiltY > 0) {
    pose.dyL += scaled(tiltY, 5);
    pose.dyR += scaled(tiltY, 5);
    if (openL) {
      pose.hL -= scaled(tiltY, 8);
    }
    if (openR) {
      pose.hR -= scaled(tiltY, 8);
    }
    pose.browL += scaled(tiltY, 2);
    pose.browR += scaled(tiltY, 2);
  } else if (tiltY < 0) {
    float s = -tiltY;
    pose.dyL -= scaled(s, 4);
    pose.dyR -= scaled(s, 4);
    if (openL) {
      pose.hL += scaled(s, 2);
    }
    if (openR) {
      pose.hR += scaled(s, 2);
    }
    pose.browL -= scaled(s, 2);
    pose.browR -= scaled(s, 2);
  }
}

Pose targetPose() {
  if (phase == Phase::Shaken) {
    Pose pose = REST;
    pose.wL = 28;
    pose.wR = 28;
    pose.hL = 38;
    pose.hR = 38;
    pose.browL = -3;
    pose.browR = -3;
    pose.archL = 6;
    pose.archR = 6;
    return pose;
  }
  // While tilted, the tilt decides where the eyes look, so random glances are skipped.
  Expr shownExpr = expr;
  bool tilted = tiltX != 0 || tiltY != 0;
  if (tilted && (expr == Expr::LookSide || expr == Expr::LookUp || expr == Expr::LookDown)) {
    shownExpr = Expr::Idle;
  }
  Pose pose = exprPose(shownExpr);
  if (phase == Phase::Awake) {
    applyTilt(pose);
    applyMood(pose);
  }
  return pose;
}

int bobAt(uint32_t now) {
  int bob = (int)((now / 140) % 8);
  if (bob > 4) {
    bob = 8 - bob;
  }
  return bob - 2;
}

// An Archimedean spiral of about two and a half turns. dir 1 or -1 mirrors it.
void drawSpiral(int cx, int cy, float spin, int dir) {
  const int steps = 60;
  int px = cx;
  int py = cy;
  for (int i = 1; i <= steps; i++) {
    float t = (float)i / steps;
    float a = dir * (t * 5.0f * PI + spin);
    float r = 14.0f * t;
    int x = cx + (int)lroundf(r * cosf(a));
    int y = cy + (int)lroundf(r * sinf(a));
    u8g2.drawLine(px, py, x, y);
    u8g2.drawLine(px + 1, py, x + 1, y);
    px = x;
    py = y;
  }
}

// Spinning spirals that drift in small circles, with wobbling brows.
void drawDizzy(uint32_t now) {
  float drift = now * 0.005f;
  float spin = now * 0.006f;
  int dx = (int)lroundf(3.0f * cosf(drift));
  int dy = (int)lroundf(2.0f * sinf(drift));
  int tilt = (int)lroundf(3.0f * sinf(now * 0.004f));
  int xL = LEFT_X + dx + (int)lroundf(bounceL.x);
  int xR = RIGHT_X - dx + (int)lroundf(bounceR.x);
  int yL = dy + (int)lroundf(bounceL.y);
  int yR = -dy + (int)lroundf(bounceR.y);
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  drawBrow(xL, BROW_Y + yL, 24, 3, tilt, true);
  drawBrow(xR, BROW_Y + yR, 24, 3, -tilt, false);
  drawSpiral(xL, EYE_Y + yL, spin, 1);
  drawSpiral(xR, EYE_Y + yR, spin, -1);
  keepBlueOnly();
}

// Eases one step toward the current pose and draws it into the framebuffer.
void faceDraw(uint32_t now) {
  if (phase == Phase::Dizzy) {
    drawDizzy(now);
    return;
  }

  Pose goal = targetPose();
  approach(shown.dxL, goal.dxL, 1);
  approach(shown.dxR, goal.dxR, 1);
  approach(shown.dyL, goal.dyL, 1);
  approach(shown.dyR, goal.dyR, 1);
  approach(shown.wL, goal.wL, 1);
  approach(shown.wR, goal.wR, 1);
  approach(shown.hL, goal.hL, 2);
  approach(shown.hR, goal.hR, 2);
  approach(shown.browL, goal.browL, 1);
  approach(shown.browR, goal.browR, 1);
  approach(shown.archL, goal.archL, 1);
  approach(shown.archR, goal.archR, 1);
  approach(shown.tiltL, goal.tiltL, 1);
  approach(shown.tiltR, goal.tiltR, 1);
  approach(shown.cheekL, goal.cheekL, 2);
  approach(shown.cheekR, goal.cheekR, 2);

  int bobL = bobAt(now) + (int)lroundf(bounceL.y);
  int bobR = bobAt(now + 80) + (int)lroundf(bounceR.y);
  int xL = LEFT_X + shown.dxL + (int)lroundf(bounceL.x);
  int xR = RIGHT_X + shown.dxR + (int)lroundf(bounceR.x);
  int browYL = BROW_Y + shown.browL + bobL;
  int browYR = BROW_Y + shown.browR + bobR;
  if (browYL < BLUE_TOP) {
    browYL = BLUE_TOP;
  }
  if (browYR < BLUE_TOP) {
    browYR = BLUE_TOP;
  }

  u8g2.clearBuffer();
  drawBrow(xL, browYL, shown.wL, shown.archL, shown.tiltL, true);
  drawBrow(xR, browYR, shown.wR, shown.archR, shown.tiltR, false);
  drawEyeShape(xL, EYE_Y + shown.dyL + bobL, shown.wL, shown.hL, shown.cheekL);
  drawEyeShape(xR, EYE_Y + shown.dyR + bobR, shown.wR, shown.hR, shown.cheekR);
  keepBlueOnly();
}

void closeEyes() {
  shown = REST;
  shown.hL = 4;
  shown.hR = 4;
}

// wokeUp: back from deep sleep, so open the eyes from closed.
void faceBegin(uint32_t now, uint8_t savedMood, bool wokeUp) {
  mood = (Mood)(savedMood % MOOD_COUNT);
  phase = Phase::Awake;
  if (wokeUp) {
    closeEyes();
  }
  enterExpr(MOOD_STEPS[(uint8_t)mood].steps[0].expr, now, 1600);
}

// step is 1 for the next mood and -1 for the previous one.
uint8_t faceStepMood(uint32_t now, int step) {
  mood = (Mood)(((uint8_t)mood + MOOD_COUNT + step) % MOOD_COUNT);
  enterExpr(MOOD_STEPS[(uint8_t)mood].steps[0].expr, now, 1200);
  Serial.printf("mood: %s\n", MOOD_NAMES[(uint8_t)mood]);
  return (uint8_t)mood;
}

bool faceAwake() {
  return phase == Phase::Awake || phase == Phase::Shaken || phase == Phase::Dizzy;
}

bool faceAsleep() {
  return phase == Phase::Asleep;
}

// Called with the latest motion reading while awake.
void faceShake(uint32_t now, bool shaking, bool dizzy) {
  if (!faceAwake()) {
    return;
  }
  if (dizzy) {
    if (phase != Phase::Dizzy) {
      Serial.println("dizzy");
    }
    phase = Phase::Dizzy;
    dizzyEnd = now + DIZZY_FOR_MS;
  } else if (shaking && phase == Phase::Awake) {
    phase = Phase::Shaken;
  } else if (!shaking && phase == Phase::Shaken) {
    phase = Phase::Awake;
  }
}

// The eyes get sleepy, then close. faceAsleep() turns true once they are shut.
void faceDoze(uint32_t now) {
  phase = Phase::Dozing;
  enterExpr(Expr::Sleepy, now, 700);
}

void faceWake(uint32_t now) {
  phase = Phase::Awake;
  enterExpr(MOOD_STEPS[(uint8_t)mood].steps[0].expr, now, 1200);
}

void faceUpdate(uint32_t now) {
  stepPhysics(now);
  if (phase == Phase::Dizzy) {
    if ((int32_t)(now - dizzyEnd) >= 0) {
      // Come out of it with the eyes shut and let them ease open.
      phase = Phase::Awake;
      closeEyes();
      enterExpr(Expr::Blink, now, 400);
    }
    return;
  }
  if ((int32_t)(now - exprEnd) < 0 || phase == Phase::Asleep) {
    return;
  }
  if (phase == Phase::Dozing) {
    if (expr == Expr::Sleepy) {
      enterExpr(Expr::Blink, now, 1200);
    } else {
      phase = Phase::Asleep;
    }
    return;
  }
  advanceExpr(now);
}
