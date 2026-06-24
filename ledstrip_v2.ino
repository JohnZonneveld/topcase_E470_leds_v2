#include <FastLED.h>

// =========================
// LED STRIP CONFIG
// =========================
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// Right side strips
#define PIN_R9 2
#define PIN_R8 4
#define PIN_R6 6

// Left side strips
#define PIN_L9 3
#define PIN_L8 5
#define PIN_L6 7

// Inputs
#define BRAKE_PIN 8
#define RIGHT_PIN 9
#define LEFT_PIN 10

// Define LED strip lengths
#define LEN9 9
#define LEN8 8
#define LEN6 6

// =========================
// BRIGHTNESS DEFINITION
// =========================
uint8_t RUN_BRIGHT   = 120;
uint8_t BRAKE_BRIGHT = 200;
uint8_t TURN_BRIGHT  = 200;

// =========================
// STRIPS DEFINITION
// =========================
CRGB r9[LEN9], r8[LEN8], r6[LEN6];
CRGB l9[LEN9], l8[LEN8], l6[LEN6];

// =========================
// INPUT FILTER
// =========================
struct Inp {
  uint8_t pin;
  bool state, last;
  unsigned long t;
};
//===================================
// SETTING INITIAL VALUES
//===================================
Inp brake = {BRAKE_PIN,false,false,0};
Inp leftI = {LEFT_PIN,false,false,0};
Inp rightI= {RIGHT_PIN,false,false,0};

const int debounceMs = 30;

// =========================
// TURN STATE
// =========================
unsigned long leftHold=0, rightHold=0;
unsigned long leftCycle=500, rightCycle=500;
unsigned long leftRise=0, rightRise=0;

bool prevL=false, prevR=false;

uint8_t leftStep=0, rightStep=0;
unsigned long lastAnim=0;

// =========================
// FONT (3x5 compressed)
// =========================
struct Ch { byte r[3]; };

Ch font(char c){
  switch(c){
    case 'F': return {{0b111,0b100,0b100}};
    case 'A': return {{0b010,0b111,0b101}};
    case 'T': return {{0b111,0b010,0b010}};
    case 'N': return {{0b101,0b111,0b101}};
    case 'I': return {{0b010,0b010,0b010}};
    case 'J': return {{0b001,0b001,0b111}};
    case ' ': return {{0,0,0}};
  }
  return {{0,0,0}};
}

// =========================================================================
// GENERIC INPUT DEBOUNCER (HANDLES MECHANIAL VIBRATION / SWITCH BOUNCE)
// =========================================================================
void updateInput(Inp &input) {
  // Read the raw electrical state of the physical pin
  bool rawReading = (digitalRead(input.pin) == HIGH);

  // If the physical pin changed state, reset the debounce timer
  if (rawReading != input.last) {
    input.t = millis();
  }

  // If the reading has been stable for longer than the debounce window
  if ((millis() - input.t) > debounceMs) {
    // Lock in the new verified state
    input.state = rawReading;
  }

  // Save the raw reading for the next processor loop comparison
  input.last = rawReading;
}

// =========================
// SETUP
// =========================
void setup(){
  pinMode(BRAKE_PIN,INPUT);
  pinMode(LEFT_PIN,INPUT);
  pinMode(RIGHT_PIN,INPUT);

  FastLED.addLeds<LED_TYPE,PIN_R9,COLOR_ORDER>(r9,LEN9);
  FastLED.addLeds<LED_TYPE,PIN_R8,COLOR_ORDER>(r8,LEN8);
  FastLED.addLeds<LED_TYPE,PIN_R6,COLOR_ORDER>(r6,LEN6);

  FastLED.addLeds<LED_TYPE,PIN_L9,COLOR_ORDER>(l9,LEN9);
  FastLED.addLeds<LED_TYPE,PIN_L8,COLOR_ORDER>(l8,LEN8);
  FastLED.addLeds<LED_TYPE,PIN_L6,COLOR_ORDER>(l6,LEN6);

  startupAnim();

  clearAll();
  FastLED.show();
}

// ===========================
// CLEAR ALL LEDs (set black)
// ===========================
void clearAll(){
  fill_solid(r9,LEN9,CRGB::Black);
  fill_solid(r8,LEN8,CRGB::Black);
  fill_solid(r6,LEN6,CRGB::Black);
  fill_solid(l9,LEN9,CRGB::Black);
  fill_solid(l8,LEN8,CRGB::Black);
  fill_solid(l6,LEN6,CRGB::Black);
}

// =========================
// TURN / BRAKE / RUN
// =========================

void comet(CRGB* s, int len, int pos){
  // i = 0 is the leading head, i = 1, 2, 3 are the trailing dim pixels
  for(int i = 0; i < 4; i++){
    
    // Core Math: Starts at the center line (len - 1) and counts outward as pos increases.
    // Adding '+ i' ensures the dim tail pixels trail behind toward the center line.
    int p = len - 1 - pos + i;
    
    // Strict boundary safety check
    if(p >= 0 && p < len) {
      // Set brightness of comet LEDs, as our comet has 4 LEDs
      // brightness is 200/(0+1) = 200, 200/(1+1) = 100, 200/(2+1) = 66 and 200/(3+1) = 50
      s[p] = CRGB(200 / (i + 1), 0 , 0); 
    }
  }
}

// A 3x18 lookup table stored safely in Flash memory (PROGMEM)
// in stead of difficult calculations because of the numbering from right to left is up-down
// we handle the 6 led-strips as one 3x18 display and do a translation via this lookup table
// to see which led has to be activated. This is only for the startup animation.
const int8_t PROGMEM layoutMap[3][18] = {
  //  0   1   2   3   4   5   6   7   8  |  9  10  11  12  13  14  15  16  17   <- Virtual X
  {   0,  1,  2,  3,  4,  5,  6,  7,  8,    8,  7,  6,  5,  4,  3,  2,  1,  0 }, // Row 0 (Top)
  {  -1,  0,  1,  2,  3,  4,  5,  6,  7,    7,  6,  5,  4,  3,  2,  1,  0, -1 }, // Row 1 (Mid)
  {  -1, -1, -1,  0,  1,  2,  3,  4,  5,    5,  4,  3,  2,  1,  0, -1, -1, -1 }  // Row 2 (Bot)
};

// =========================================================================
// UNIFIED GRAPHICS MAPPING ENGINE (MANAGES HARDWARE LAYOUT)
// =========================================================================
void setMatrixPixel(int x, int row, CRGB color) {
  // Safety boundary check, if any x or row value is bigger than what we have --> stop executing
  if (x < 0 || x > 17 || row < 0 || row > 2) return;

  // Read the physical index from the flash memory table
  int8_t physicalIdx = pgm_read_byte(&(layoutMap[row][x]));

  // If it's a phantom/empty slot (-1), discard it immediately!
  if (physicalIdx == -1) return;

  // Send the color to the correct strip array based on left/right hemisphere
  if (x <= 8) {
    if (row == 0)      l9[physicalIdx] = color;
    else if (row == 1) l8[physicalIdx] = color;
    else if (row == 2) l6[physicalIdx] = color;
  } 
  else {
    if (row == 0)      r9[physicalIdx] = color;
    else if (row == 1) r8[physicalIdx] = color;
    else if (row == 2) r6[physicalIdx] = color;
  }
}

// =========================================================================
// STARTUP ANIMATION (ANIMATED TEXT)
// =========================================================================
void startupAnim(){
  const char* msg = " FAT NINJA ";
  int len = strlen(msg);

  // Smooth right-to-left marquee scan across the full 18-column virtual grid
  for(int scan = 20; scan > -len * 4; scan--){
    clearAll();

    for(int i = 0; i < len; i++){
      Ch c = font(msg[i]);

      for(int col = 0; col < 3; col++){
        int x = scan + i * 4 + col;

        for(int row = 0; row < 3; row++){
          // Read font columns naturally from left to right
          if(c.r[row] & (1 << (2 - col))){
            // Pass coordinates straight to our mapping engine
            setMatrixPixel(x, row, CRGB(180, 0, 0));
          }
        }
      }
    }

    FastLED.show();
    // as it is the startup animation we can use a delay to 'slow' down the scroll
    // we have 9 characters including 1 space and one empty space between characters
    // so in total 9 x 4 steps = 36 steps to display the text. We only have 18 LEDs
    // Text will fill the display in 18 steps and the last character will clear after
    // and addtional 36 steps (text length), in total 54 steps. As shown below delay is 60ms
    // thus the scrolling text will be finished after 54 x 60 = 3240msec.
    // With the additional brake flashes the whole startup lasts under 3.5 sec.
    delay(60); 
  }

  // Ignition flashes rewritten to use the discrete hardware strips
  for(int i = 0; i < 2; i++){
    fill_solid(r9, LEN9, CRGB(255,0,0));
    fill_solid(r8, LEN8, CRGB(255,0,0));
    fill_solid(r6, LEN6, CRGB(255,0,0));
    fill_solid(l9, LEN9, CRGB(255,0,0));
    fill_solid(l8, LEN8, CRGB(255,0,0));
    fill_solid(l6, LEN6, CRGB(255,0,0));
    FastLED.show(); 
    delay(120);
    
    clearAll();
    FastLED.show(); 
    delay(120);
  }
}

// =========================
// LOOP
// =========================
void loop() {
  // REFRESH DEBOUNCED INPUT STRUCTURES
  updateInput(brake);
  updateInput(leftI);
  updateInput(rightI);

  unsigned long now = millis();

  // Track the physical pulses for coincidence hazard detection
  static unsigned long lastLeftRiseTime = 0;
  static unsigned long lastRightRiseTime = 0;

  // ====================================================================================
  // INVERTED PHASE EDGE DETECTION & STATE LATCHING, TO GET LEDs COMET IN SYNC WITH BIKE
  // ====================================================================================
  static bool leftLatch = false;
  static bool rightLatch = false;

  // LEFT SIDE: Reset to 0 on the FALLING edge (wire drops low) to fix phase offset
  if (!leftI.state && leftI.last) {
    lastLeftRiseTime = now;
    leftStep = 0; // Force sync to match the physical bulb rhythm
  }
  if (leftI.state) {
    leftLatch = true;
    leftHold = now + 950; // Keeps running through the dark cycle smoothly
  }
  if (now > leftHold) {
    leftLatch = false;
  }

  // RIGHT SIDE: Reset to 0 on the FALLING edge (wire drops low) to fix phase offset
  if (!rightI.state && rightI.last) {
    lastRightRiseTime = now;
    rightStep = 0; // Force sync to match the physical bulb rhythm
  }
  if (rightI.state) {
    rightLatch = true;
    rightHold = now + 950;
  }
  if (now > rightHold) {
    rightLatch = false;
  }

  // Evaluate Hazard state based on the calculated timestamps
  bool isHazard = false;
  if ((now - lastLeftRiseTime < 850) && (now - lastRightRiseTime < 850)) {
    isHazard = true;
  }

  // =========================================================================
  // MASTER SAFETY INTERLOCK (TOTAL BRAKE LOCKOUT)
  // =========================================================================
  bool leftSideShouldBrake  = brake.state;
  bool rightSideShouldBrake = brake.state;

  if (isHazard) {
    leftSideShouldBrake  = false;
    rightSideShouldBrake = false;
  } else {
    if (leftLatch)  leftSideShouldBrake  = false; // Left signaling? Strip brake input.
    if (rightLatch) rightSideShouldBrake = false; // Right signaling? Strip brake input.
  }

  // =========================================================================
  // RENDERING ENGINE
  // =========================================================================
  clearAll(); // Wipe canvas clean before mapping this frame

  // -------------------------------------------------------------------------
  // LEFT SIDE TAILLIGHT
  // -------------------------------------------------------------------------
  if (isHazard || leftLatch) {
    if (leftSideShouldBrake) {
      fill_solid(l9, LEN9, CRGB(255, 0, 0));
      fill_solid(l8, LEN8, CRGB(255, 0, 0));
      fill_solid(l6, LEN6, CRGB(255, 0, 0));
    }
    comet(l9, LEN9, leftStep);
    comet(l8, LEN8, leftStep);
    comet(l6, LEN6, leftStep);
  } 
  else if (leftSideShouldBrake) {
    fill_solid(l9, LEN9, CRGB(255, 0, 0));
    fill_solid(l8, LEN8, CRGB(255, 0, 0));
    fill_solid(l6, LEN6, CRGB(255, 0, 0));
  } 
  else {
    // Cruising state -> Outer contour accents only
    for(int i = 0; i < 4; i++) l9[i] = CRGB(60, 0, 0);
    for(int i = 0; i < 3; i++) l8[i] = CRGB(60, 0, 0);
    l6[0] = CRGB(60, 0, 0);
  }

  // -------------------------------------------------------------------------
  // RIGHT SIDE TAILLIGHT
  // -------------------------------------------------------------------------
  if (isHazard || rightLatch) {
    if (rightSideShouldBrake) {
      fill_solid(r9, LEN9, CRGB(255, 0, 0));
      fill_solid(r8, LEN8, CRGB(255, 0, 0));
      fill_solid(r6, LEN6, CRGB(255, 0, 0));
    }
    comet(r9, LEN9, rightStep);
    comet(r8, LEN8, rightStep);
    comet(r6, LEN6, rightStep);
  } 
  else if (rightSideShouldBrake) {
    fill_solid(r9, LEN9, CRGB(255, 0, 0));
    fill_solid(r8, LEN8, CRGB(255, 0, 0));
    fill_solid(r6, LEN6, CRGB(255, 0, 0));
  } 
  else {
    for(int i = 0; i < 4; i++) r9[i] = CRGB(60, 0, 0);
    for(int i = 0; i < 3; i++) r8[i] = CRGB(60, 0, 0);
    r6[0] = CRGB(60, 0, 0);
  }

  // =========================================================================
  // ANIMATION TIMING ENGINE
  // =========================================================================
  static unsigned long lastStepTime = 0;
  
  // Tweak the 40ms frame delay lower if you want a faster, snappier sweep
  if (now - lastStepTime >= 40) {
    lastStepTime = now;

    if (leftLatch || isHazard) {
      if (leftStep < 12) leftStep++; 
    } else {
      leftStep = 0;
    }

    if (rightLatch || isHazard) {
      if (rightStep < 12) rightStep++;
    } else {
      rightStep = 0;
    }
  }

  // TRANSMIT DATA GENERATION OUT TO THE PHYSICAL STRIPS
  FastLED.show();
}
