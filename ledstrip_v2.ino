#include <FastLED.h>

// ---------------------- Configuration ----------------------
#define LED_TYPE     WS2812B
#define COLOR_ORDER  GRB

static const uint8_t NUM_LEFT_STRIPS  = 3;
static const uint8_t NUM_RIGHT_STRIPS = 3;
static const uint8_t MAX_LEN = 9; // max of your 9/8/6

static const uint8_t leftPins[NUM_LEFT_STRIPS]    = {2, 3, 4};
static const uint8_t rightPins[NUM_RIGHT_STRIPS]  = {5, 6, 7};

static const uint8_t leftLen[NUM_LEFT_STRIPS]     = {9, 8, 6};
static const uint8_t rightLen[NUM_RIGHT_STRIPS]   = {9, 8, 6};

static const uint8_t PIN_BRAKE = 8;
static const uint8_t PIN_LEFT  = 9;
static const uint8_t PIN_RIGHT = 10;

static const uint8_t RUNNING_LEVEL = 128; // 50%
static const uint8_t BRAKE_LEVEL   = 255;
static const uint8_t TURN_LEVEL    = 255;

static const uint16_t DEBOUNCE_MS         = 10;
static const uint16_t DEFAULT_TURN_ON_MS  = 450;
static const uint16_t MIN_STEP_MS         = 15;
static const uint16_t MAX_STEP_MS         = 250;

// Tail/Brake/Turn color
static const CRGB BASE_COLOR = CRGB(255, 0, 0);
// -----------------------------------------------------------

CRGB leftLeds[NUM_LEFT_STRIPS][MAX_LEN];
CRGB rightLeds[NUM_RIGHT_STRIPS][MAX_LEN];

struct DebouncedInput {
  uint8_t pin;
  bool stableState = false;
  bool lastRaw = false;
  uint32_t lastChangeMs = 0;

  void begin(uint8_t p) {
    pin = p;
    pinMode(pin, INPUT); // use INPUT_PULLUP if needed (then invert read)
    lastRaw = digitalRead(pin);
    stableState = lastRaw;
    lastChangeMs = millis();
  }

  bool update() {
    bool raw = digitalRead(pin);
    uint32_t now = millis();
    if (raw != lastRaw) {
      lastRaw = raw;
      lastChangeMs = now;
    }
    if ((now - lastChangeMs) >= DEBOUNCE_MS) stableState = lastRaw;
    return stableState;
  }
};

DebouncedInput inBrake, inLeft, inRight;

struct TurnAnimator {
  bool lastState = false;
  uint32_t lastEdgeMs = 0;

  uint16_t lastOnMs  = DEFAULT_TURN_ON_MS;
  uint16_t lastOffMs = DEFAULT_TURN_ON_MS;

  uint8_t maxLen = 9;
  uint8_t fillCount = 0;
  uint32_t nextStepMs = 0;
  uint16_t stepIntervalMs = 50;

  void begin(uint8_t maxStripLen) {
    maxLen = maxStripLen;
    lastState = false;
    lastEdgeMs = millis();
    fillCount = 0;
    nextStepMs = millis();
    computeStepInterval();
  }

  void computeStepInterval() {
    uint16_t interval = (uint16_t)(lastOnMs / (maxLen ? maxLen : 1));
    interval = constrain(interval, MIN_STEP_MS, MAX_STEP_MS);
    stepIntervalMs = interval;
  }

  void updateRhythm(bool currentState) {
    uint32_t now = millis();
    if (currentState != lastState) {
      uint32_t dt = now - lastEdgeMs;
      lastEdgeMs = now;

      if (lastState == true && currentState == false) {
        // ON ended
        if (dt >= 20 && dt <= 3000) lastOnMs = (uint16_t)dt;
        computeStepInterval();
      } else if (lastState == false && currentState == true) {
        // OFF ended (ON started)
        if (dt >= 20 && dt <= 3000) lastOffMs = (uint16_t)dt;
        fillCount = 0;
        nextStepMs = now;
      }
      lastState = currentState;
    }
  }

  void updateAnimation(bool signalOn) {
    uint32_t now = millis();
    if (!signalOn) { fillCount = 0; return; }

    if ((int32_t)(now - nextStepMs) >= 0) {
      if (fillCount < maxLen) fillCount++;
      else fillCount = 1; // repeat if ON stays long
      nextStepMs = now + stepIntervalMs;
    }
  }
};

TurnAnimator leftAnim, rightAnim;

static inline CRGB scaledColor(uint8_t level) {
  CRGB c = BASE_COLOR;
  c.nscale8_video(level);
  return c;
}

void clearAll() {
  for (uint8_t s = 0; s < NUM_LEFT_STRIPS; s++)  fill_solid(leftLeds[s],  MAX_LEN, CRGB::Black);
  for (uint8_t s = 0; s < NUM_RIGHT_STRIPS; s++) fill_solid(rightLeds[s], MAX_LEN, CRGB::Black);
}

void setSideSolid(CRGB side[][MAX_LEN], const uint8_t *lens, uint8_t nStrips, uint8_t level) {
  CRGB c = scaledColor(level);
  for (uint8_t s = 0; s < nStrips; s++) {
    for (uint8_t i = 0; i < lens[s]; i++) side[s][i] = c;
  }
}

// Outside running lights = last 3 indices (because inside is index 0)
void setSideRunningOutside3(CRGB side[][MAX_LEN], const uint8_t *lens, uint8_t nStrips, uint8_t level) {
  CRGB c = scaledColor(level);
  for (uint8_t s = 0; s < nStrips; s++) {
    uint8_t L = lens[s];
    uint8_t start = (L > 3) ? (L - 3) : 0;
    for (uint8_t i = start; i < L; i++) side[s][i] = c;
  }
}

// Turn fill from inside (0) outward
void setSideTurnFill(CRGB side[][MAX_LEN], const uint8_t *lens, uint8_t nStrips, uint8_t fillCount, uint8_t level) {
  CRGB c = scaledColor(level);
  for (uint8_t s = 0; s < nStrips; s++) {
    uint8_t L = lens[s];
    uint8_t count = (fillCount > L) ? L : fillCount;
    for (uint8_t i = 0; i < L; i++) side[s][i] = (i < count) ? c : CRGB::Black;
  }
}

void setup() {
  // Add each strip controller (each pin is separate)
  FastLED.addLeds<LED_TYPE, 2, COLOR_ORDER>(leftLeds[0], MAX_LEN);
  FastLED.addLeds<LED_TYPE, 3, COLOR_ORDER>(leftLeds[1], MAX_LEN);
  FastLED.addLeds<LED_TYPE, 4, COLOR_ORDER>(leftLeds[2], MAX_LEN);

  FastLED.addLeds<LED_TYPE, 5, COLOR_ORDER>(rightLeds[0], MAX_LEN);
  FastLED.addLeds<LED_TYPE, 6, COLOR_ORDER>(rightLeds[1], MAX_LEN);
  FastLED.addLeds<LED_TYPE, 7, COLOR_ORDER>(rightLeds[2], MAX_LEN);

  inBrake.begin(PIN_BRAKE);
  inLeft.begin(PIN_LEFT);
  inRight.begin(PIN_RIGHT);

  uint8_t maxLeft = 0, maxRight = 0;
  for (uint8_t i = 0; i < NUM_LEFT_STRIPS; i++)  maxLeft  = max(maxLeft,  leftLen[i]);
  for (uint8_t i = 0; i < NUM_RIGHT_STRIPS; i++) maxRight = max(maxRight, rightLen[i]);

  leftAnim.begin(maxLeft);
  rightAnim.begin(maxRight);

  FastLED.clear(true);
}

void loop() {
  bool brake = inBrake.update();
  bool left  = inLeft.update();
  bool right = inRight.update();

  leftAnim.updateRhythm(left);
  rightAnim.updateRhythm(right);
  leftAnim.updateAnimation(left);
  rightAnim.updateAnimation(right);

  bool hazard = left && right;

  clearAll();

  if (brake && !left && !right) {
    // Brake only
    setSideSolid(leftLeds, leftLen, NUM_LEFT_STRIPS, BRAKE_LEVEL);
    setSideSolid(rightLeds, rightLen, NUM_RIGHT_STRIPS, BRAKE_LEVEL);
  }
  else if (hazard) {
    // Hazard: animate both sides
    setSideTurnFill(leftLeds, leftLen, NUM_LEFT_STRIPS, leftAnim.fillCount, TURN_LEVEL);
    setSideTurnFill(rightLeds, rightLen, NUM_RIGHT_STRIPS, rightAnim.fillCount, TURN_LEVEL);
  }
  else if (brake && (left || right)) {
    // Brake + turn: other side solid
    if (left) {
      setSideTurnFill(leftLeds, leftLen, NUM_LEFT_STRIPS, leftAnim.fillCount, TURN_LEVEL);
      setSideSolid(rightLeds, rightLen, NUM_RIGHT_STRIPS, BRAKE_LEVEL);
    } else {
      setSideSolid(leftLeds, leftLen, NUM_LEFT_STRIPS, BRAKE_LEVEL);
      setSideTurnFill(rightLeds, rightLen, NUM_RIGHT_STRIPS, rightAnim.fillCount, TURN_LEVEL);
    }
  }
  else if (!brake && (left || right)) {
    // Turn only: other side running outside-3
    if (left) {
      setSideTurnFill(leftLeds, leftLen, NUM_LEFT_STRIPS, leftAnim.fillCount, TURN_LEVEL);
      setSideRunningOutside3(rightLeds, rightLen, NUM_RIGHT_STRIPS, RUNNING_LEVEL);
    } else {
      setSideRunningOutside3(leftLeds, leftLen, NUM_LEFT_STRIPS, RUNNING_LEVEL);
      setSideTurnFill(rightLeds, rightLen, NUM_RIGHT_STRIPS, rightAnim.fillCount, TURN_LEVEL);
    }
  }
  else {
    // Rest
    setSideRunningOutside3(leftLeds, leftLen, NUM_LEFT_STRIPS, RUNNING_LEVEL);
    setSideRunningOutside3(rightLeds, rightLen, NUM_RIGHT_STRIPS, RUNNING_LEVEL);
  }

  FastLED.show();
}
