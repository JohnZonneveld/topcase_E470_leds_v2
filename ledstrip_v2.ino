#include <FastLED.h>

// =========================
// LED CONFIG
// =========================
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// Right side
#define PIN_R9 2
#define PIN_R8 4
#define PIN_R6 6

// Left side
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
// BRIGHTNESS
// =========================
uint8_t RUN_BRIGHT   = 120;
uint8_t BRAKE_BRIGHT = 200;
uint8_t TURN_BRIGHT  = 200;

// =========================
// STRIPS
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

// =========================
// PIXEL DRAW (REAL HARDWARE MAPPING)
// =========================
void drawPixel(int x,int y,CRGB c){

  if(y==0){ // top (9)
    int i = x - offTop;
    if(i>=0 && i<LEN9){ r9[i]=l9[i]=c; }
  }

  if(y==1){ // mid (8)
    int i = x - offMid;
    if(i>=0 && i<LEN8){ r8[i]=l8[i]=c; }
  }

  if(y==2){ // bottom (6)
    int i = x - offBot;
    if(i>=0 && i<LEN6){ r6[i]=l6[i]=c; }
  }
}

// =========================
// INPUT
// =========================
void upd(Inp &i){
  bool r=digitalRead(i.pin);
  if(r!=i.last){i.t=millis();i.last=r;}
  if(millis()-i.t>debounceMs)i.state=r;
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

  FastLED.clear();
  FastLED.show();

  startupAnim();
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

void comet(CRGB* s,int len,int pos){
  for(int i=0;i<4;i++){
    int p=len-1-pos+i;
    if(p>=0&&p<len) s[p]=CRGB(200/(i+1),0,0);
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

// =========================
// STARTUP ANIMATION (FIXED SCANLINE)
// =========================
void startupAnim(){

  const char* msg = " FAT NINJA ";
  int len = strlen(msg);

  for(int scan=20; scan > -len*4; scan--){

    clearAll();

    for(int i=0;i<len;i++){
      Ch c = font(msg[i]);

      for(int col=0; col<3; col++){
        int x = scan + i*4 + col;

        for(int row=0; row<3; row++){
          if(c.r[row] & (1 << (2-col))){
            drawPixel(x,row,CRGB(180,0,0));
          }
        }
      }
    }

    FastLED.show();
    delay(60);
  }

  // ignition flash
  for(int i=0;i<2;i++){
    fill_solid(r9,LEN9,CRGB(255,0,0));
    fill_solid(r8,LEN8,CRGB(255,0,0));
    fill_solid(r6,LEN6,CRGB(255,0,0));
    fill_solid(l9,LEN9,CRGB(255,0,0));
    fill_solid(l8,LEN8,CRGB(255,0,0));
    fill_solid(l6,LEN6,CRGB(255,0,0));

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

  upd(brake);
  upd(leftI);
  upd(rightI);

  bool b=brake.state;
  bool L=leftI.state;
  bool R=rightI.state;

  unsigned long now=millis();

  if(L&&!prevL){
    leftCycle=now-leftRise;
    leftRise=now;
    leftStep=0;
    leftHold=now+leftCycle;
  }

  if(R&&!prevR){
    rightCycle=now-rightRise;
    rightRise=now;
    rightStep=0;
    rightHold=now+rightCycle;
  }

  prevL=L;
  prevR=R;

  bool La=L||now<leftHold;
  bool Ra=R||now<rightHold;
  bool haz=La&&Ra;

  if(now-lastAnim > max(20UL,min(leftCycle,rightCycle)/10)){
    lastAnim=now;
    if(La) leftStep++;
    if(Ra) rightStep++;
  }

  clearAll();

  // RUNNING
  if(!b){
    if(!haz){
      if(!La) runLight(true);
      if(!Ra) runLight(false);
    }
  }

  // BRAKE
  if(b){
    if(haz){
      brakeLight(true);
      brakeLight(false);
    } else {
      if(!La) brakeLight(true);
      if(!Ra) brakeLight(false);
    }
  }

  // TURN
  if(haz){
    turn(true,leftStep);
    turn(false,rightStep);
  } else {
    if(La) turn(true,leftStep);
    if(Ra) turn(false,rightStep);
  }

  FastLED.show();
}
