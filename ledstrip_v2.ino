#include <FastLED.h>

// =========================
// LED CONFIG
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
// ROW ALIGNMENT OFFSETS
// (THIS FIXES YOUR PHYSICAL SHIFT)
// =========================
int offTop = -3;   // 9 LED row
int offMid = -2;   // 8 LED row
int offBot =  0;   // 6 LED row

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

// =========================
// CLEAR
// =========================
void clearAll(){
  fill_solid(r9,LEN9,CRGB::Black);
  fill_solid(r8,LEN8,CRGB::Black);
  fill_solid(r6,LEN6,CRGB::Black);
  fill_solid(l9,LEN9,CRGB::Black);
  fill_solid(l8,LEN8,CRGB::Black);
  fill_solid(l6,LEN6,CRGB::Black);
}

// =========================
// TURN / BRAKE
// =========================
void runLight(bool left){
  CRGB c = CRGB(RUN_BRIGHT,0,0);

  if(left){
    l6[0]=c;
    for(int i=0;i<3;i++) l8[i]=c;
    for(int i=0;i<4;i++) l9[i]=c;
  } else {
    r6[0]=c;
    for(int i=0;i<3;i++) r8[i]=c;
    for(int i=0;i<4;i++) r9[i]=c;
  }
}

void brakeLight(bool left){
  CRGB c = CRGB(BRAKE_BRIGHT,0,0);

  if(left){
    fill_solid(l6,LEN6,c);
    fill_solid(l8,LEN8,c);
    fill_solid(l9,LEN9,c);
  } else {
    fill_solid(r6,LEN6,c);
    fill_solid(r8,LEN8,c);
    fill_solid(r9,LEN9,c);
  }
}

void comet(CRGB* s, int len, int pos){
  // i = 0 is the leading head, i = 1, 2, 3 are the trailing dim pixels
  for(int i = 0; i < 4; i++){
    
    // Core Math: Starts at the center line (len - 1) and counts outward as pos increases.
    // Adding '+ i' ensures the dim tail pixels trail behind toward the center line.
    int p = len - 1 - pos + i;
    
    // Strict boundary safety check
    if(p >= 0 && p < len) {
      // Vivid automotive amber
      s[p] = CRGB(200 / (i + 1), 0 , 0); 
    }
  }
}

void turn(bool leftSide,uint8_t step){
  if(leftSide){
    comet(l6,LEN6,step);
    comet(l8,LEN8,step);
    comet(l9,LEN9,step);
  } else {
    comet(r6,LEN6,step);
    comet(r8,LEN8,step);
    comet(r9,LEN9,step);
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
  // 1. Safety boundary check
  if (x < 0 || x > 17 || row < 0 || row > 2) return;

  // 2. Read the physical index from the flash memory table
  int8_t physicalIdx = pgm_read_byte(&(layoutMap[row][x]));

  // 3. If it's a phantom/empty slot (-1), discard it immediately!
  if (physicalIdx == -1) return;

  // 4. Send the color to the correct strip array based on left/right hemisphere
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
// STARTUP ANIMATION (FLAWLESS UNIFIED HORIZONTAL MARXCH)
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
void loop(){

  updateInput(brake);
  updateInput(leftI);
  updateInput(rightI);

  bool isBraking=brake.state;
  // bool isHazard = isLeftActive && isRightActive // disabled this because of hazards shadowing over brakelight
  bool isLeftOn = leftI.state; // && !isHazard;
  bool isRightOn = rightI.state; // && !isHazard;

  unsigned long now=millis();

  // Left Signal Edge Detection & Dynamic Sync
  if(isLeftOn && !prevL){
    if (leftRise != 0) { 
      unsigned long calc = now - leftRise;
      // Sanity check: normal motorcycle flashers run between 50Hz and 120Hz 
      // (approx 500ms to 1200ms per full cycle)
      if (calc > 400 && calc < 1200) {
        leftCycle = calc; 
      }
    }
    leftRise = now;
    leftStep = 0;
    
    // Set hold time to 1.3x the actual measured cycle.
    // This guarantees it covers the "off" phase, but times out quickly when canceled.
    leftHold = now + ((leftCycle * 13) / 10); 
  }

  // Right Signal Edge Detection & Dynamic Sync
  if(isRightOn && !prevR){
    if (rightRise != 0) {
      unsigned long calc = now - rightRise;
      if (calc > 400 && calc < 1200) {
        rightCycle = calc;
      }
    }
    rightRise = now;
    rightStep = 0;
    rightHold = now + ((rightCycle * 13) / 10);
  }

  prevL = isLeftOn;
  prevR = isRightOn;

  // Active check: Stay in turn mode if the physical pin is HIGH, 
  // OR if we are currently riding out the dark gap calculated above.
  bool isLeftActive = isLeftOn || (now < leftHold);
  bool isRightActive = isRightOn || (now < rightHold);
  bool isHazard = isLeftActive && isRightActive;

  if(now-lastAnim > max(20UL,min(leftCycle,rightCycle)/10)){
    lastAnim=now;
    if(isLeftActive) leftStep++;
    if(isRightActive) rightStep++;
  }

  clearAll();

  // RUNNING
  if(!isBraking){
    if(!isHazard){
      if(!isLeftActive) runLight(true);
      if(!isRightActive) runLight(false);
    }
  }

  // BRAKE
  if(isBraking){
    if(isHazard){
      brakeLight(true);
      brakeLight(false);
    } else {
      if(!isLeftActive) brakeLight(true);
      if(!isRightActive) brakeLight(false);
    }
  }

  // TURN
  if(isHazard){
    turn(true,leftStep);
    turn(false,rightStep);
  } else {
    if(isLeftActive) turn(true,leftStep);
    if(isRightActive) turn(false,rightStep);
  }

  FastLED.show();
}
