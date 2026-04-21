#include <FastLED.h>

// ======================================================
// CONFIG
// ======================================================
#define NUM_STRIPS 6
#define LEDS_PER_STRIP 9

#define RIGHT_PIN_1 2
#define RIGHT_PIN_2 4
#define RIGHT_PIN_3 6

#define LEFT_PIN_1 3
#define LEFT_PIN_2 5
#define LEFT_PIN_3 7

#define BRAKE_PIN 8
#define LEFT_INPUT 9
#define RIGHT_INPUT 10

CRGB leds[NUM_STRIPS][LEDS_PER_STRIP];

// ======================================================
// MODE SYSTEM (SINGLE SOURCE OF TRUTH)
// ======================================================
enum Mode {
  IDLE,
  LEFT,
  RIGHT,
  HAZARD,
  BRAKE
};

Mode mode;

// ======================================================
// SIGNAL FILTERING
// ======================================================
const unsigned long debounceTime = 40;

bool rawLeftPrev = false;
bool rawRightPrev = false;

bool stableLeft = false;
bool stableRight = false;

unsigned long leftChangeTime = 0;
unsigned long rightChangeTime = 0;

// ======================================================
// EDGE + LATCH
// ======================================================
bool leftLatched = false;
bool rightLatched = false;

bool leftPrevClean = false;
bool rightPrevClean = false;

// ======================================================
// ANIMATION
// ======================================================
int headPos = 0;
unsigned long lastStep = 0;

// ======================================================
// STARTUP
// ======================================================
bool startupDone = false;
int startupStep = 0;
unsigned long startupTimer = 0;

// ======================================================
// HELPERS
// ======================================================
int rev(int i) {
  return (LEDS_PER_STRIP - 1) - i;
}

// ======================================================
// INPUT FILTER
// ======================================================
void filterInputs(bool rawLeft, bool rawRight,
                  bool &leftOut, bool &rightOut) {

  unsigned long now = millis();

  if (rawLeft != rawLeftPrev) {
    rawLeftPrev = rawLeft;
    leftChangeTime = now;
  }

  if ((now - leftChangeTime) > debounceTime) {
    stableLeft = rawLeft;
  }

  if (rawRight != rawRightPrev) {
    rawRightPrev = rawRight;
    rightChangeTime = now;
  }

  if ((now - rightChangeTime) > debounceTime) {
    stableRight = rawRight;
  }

  leftOut = stableLeft;
  rightOut = stableRight;
}

// ======================================================
// EDGE DETECTION (ONE SHOT LATCH)
// ======================================================
void detectEdges(bool left, bool right) {

  if (left && !leftPrevClean) {
    leftLatched = true;
    headPos = 0;
  }

  if (!left) leftLatched = false;

  if (right && !rightPrevClean) {
    rightLatched = true;
    headPos = 0;
  }

  if (!right) rightLatched = false;

  leftPrevClean = left;
  rightPrevClean = right;
}

// ======================================================
// STARTUP SEQUENCE
// ======================================================
void startupSequence() {

  if (millis() - startupTimer > 25) {
    startupTimer = millis();
    startupStep++;

    if (startupStep > LEDS_PER_STRIP) {
      startupDone = true;
      return;
    }
  }

  for (int s = 0; s < NUM_STRIPS; s++) {
    for (int i = 0; i < startupStep; i++) {
      leds[s][rev(i)] = CRGB(80, 0, 0);
    }
  }

  FastLED.show();
}

// ======================================================
// RUNNING LIGHTS (MODE-AWARE)
// ======================================================
void runningLights(Mode mode) {

  // 🚨 HARD BLOCK IN HAZARD
  if (mode == HAZARD) return;

  bool leftActive = (mode == LEFT);
  bool rightActive = (mode == RIGHT);

  if (!rightActive) {
    for (int s = 0; s < 3; s++)
      for (int i = 0; i < 3; i++)
        leds[s][i] = CRGB(40, 0, 0);
  }

  if (!leftActive) {
    for (int s = 3; s < 6; s++)
      for (int i = 0; i < 3; i++)
        leds[s][i] = CRGB(40, 0, 0);
  }
}

// ======================================================
// AUDI COMET ANIMATION
// ======================================================
void audiComet(int startStrip, int endStrip, unsigned long period) {

  unsigned long stepTime = period / LEDS_PER_STRIP;

  if (millis() - lastStep > stepTime) {
    lastStep = millis();

    headPos++;

    if (headPos >= LEDS_PER_STRIP) {
      headPos = 0;
    }
  }

  for (int s = startStrip; s <= endStrip; s++) {
    for (int i = 0; i < LEDS_PER_STRIP; i++) {

      int r = rev(i);
      int dist = headPos - i;

      if (dist == 0) leds[s][r] = CRGB::Red;
      else if (dist == 1) leds[s][r] = CRGB(120, 0, 0);
      else if (dist == 2) leds[s][r] = CRGB(40, 0, 0);
    }
  }
}

// ======================================================
// BRAKE
// ======================================================
void brakeLayer() {
  for (int s = 0; s < NUM_STRIPS; s++)
    for (int i = 0; i < LEDS_PER_STRIP; i++)
      leds[s][i] = CRGB::Red;
}

// ======================================================
// SETUP
// ======================================================
void setup() {

  FastLED.addLeds<WS2812B, RIGHT_PIN_1, GRB>(leds[0], LEDS_PER_STRIP);
  FastLED.addLeds<WS2812B, RIGHT_PIN_2, GRB>(leds[1], LEDS_PER_STRIP);
  FastLED.addLeds<WS2812B, RIGHT_PIN_3, GRB>(leds[2], LEDS_PER_STRIP);

  FastLED.addLeds<WS2812B, LEFT_PIN_1, GRB>(leds[3], LEDS_PER_STRIP);
  FastLED.addLeds<WS2812B, LEFT_PIN_2, GRB>(leds[4], LEDS_PER_STRIP);
  FastLED.addLeds<WS2812B, LEFT_PIN_3, GRB>(leds[5], LEDS_PER_STRIP);
}

// ======================================================
// LOOP (SINGLE-RENDER BCM ARCHITECTURE)
// ======================================================
void loop() {

  bool rawLeft = digitalRead(LEFT_INPUT);
  bool rawRight = digitalRead(RIGHT_INPUT);
  bool brake = digitalRead(BRAKE_PIN);

  bool left, right;

  // STEP 1: FILTER INPUTS
  filterInputs(rawLeft, rawRight, left, right);

  // STEP 2: EDGE DETECTION
  detectEdges(left, right);

  // STEP 3: CLEAR FRAME
  for (int s = 0; s < NUM_STRIPS; s++)
    for (int i = 0; i < LEDS_PER_STRIP; i++)
      leds[s][i] = CRGB::Black;

  // ======================================================
  // STARTUP OVERRIDE
  // ======================================================
  if (!startupDone) {
    startupSequence();
    return;
  }

  // ======================================================
  // MODE ARBITRATION (SINGLE AUTHORITY)
  // ======================================================
  if (brake && !leftLatched && !rightLatched) {
    mode = BRAKE;
  }
  else if (leftLatched && rightLatched) {
    mode = HAZARD;
  }
  else if (leftLatched) {
    mode = LEFT;
  }
  else if (rightLatched) {
    mode = RIGHT;
  }
  else {
    mode = IDLE;
  }

  // ======================================================
  // RENDER ENGINE (NO LAYER CONFLICTS)
  // ======================================================
  switch (mode) {

    case HAZARD:
      audiComet(0, 5, 600);
      if (brake) brakeLayer();
      break;

    case LEFT:
      audiComet(3, 5, 600);
      runningLights(LEFT);
      if (brake) {
        for (int s = 0; s < 3; s++)
          for (int i = 0; i < LEDS_PER_STRIP; i++)
            leds[s][i] = CRGB::Red;
      }
      break;

    case RIGHT:
      audiComet(0, 2, 600);
      runningLights(RIGHT);
      if (brake) {
        for (int s = 3; s < 6; s++)
          for (int i = 0; i < LEDS_PER_STRIP; i++)
            leds[s][i] = CRGB::Red;
      }
      break;

    case BRAKE:
      brakeLayer();
      break;

    case IDLE:
      runningLights(IDLE);
      break;
  }

  FastLED.show();
}
