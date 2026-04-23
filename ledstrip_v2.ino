#include <FastLED.h>

// =========================
// LED CONFIG
// =========================
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// Pins (Right side)
#define PIN_R9 2
#define PIN_R8 4
#define PIN_R6 6

// Pins (Left side)
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
// STRIPS
// =========================
CRGB r9[LEN9], r8[LEN8], r6[LEN6];
CRGB l9[LEN9], l8[LEN8], l6[LEN6];

// =========================
// ROW OFFSETS (physical alignment fix)
// =========================
int offTop = -3;
int offMid = -2;
int offBot =  0;

// =========================
// FONT 3x5
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
// PIXEL DRAW (NO RIGHT SHIFT COMPENSATION)
// =========================
void drawPixel(int x,int y,CRGB c){

  // TOP ROW
  if(y==0){
    int i = x + 3;
    if(i>=0 && i<LEN9){
      r9[i]=c;
      l9[i]=c;
    }
  }

  // MIDDLE ROW
  if(y==1){
    int i = x + 2;
    if(i>=0 && i<LEN8){
      r8[i]=c;
      l8[i]=c;
    }
  }

  // BOTTOM ROW
  if(y==2){
    int i = x;
    if(i>=0 && i<LEN6){
      r6[i]=c;
      l6[i]=c;
    }
  }
}

// =========================
// CLEAR
// =========================
void clearAll(){
  fill_solid(r9,9,CRGB::Black);
  fill_solid(r8,8,CRGB::Black);
  fill_solid(r6,6,CRGB::Black);
  fill_solid(l9,9,CRGB::Black);
  fill_solid(l8,8,CRGB::Black);
  fill_solid(l6,6,CRGB::Black);
}

// =========================
// DRAW CHAR
// =========================
void drawChar(char c,int x0){
  Ch ch = font(c);

  for(int col=0; col<3; col++){
    int x = x0 + col;

    for(int row=0; row<3; row++){
      if(ch.r[row] & (1 << (2-col))){
        drawPixel(x,row,CRGB(180,0,0));
      }
    }
  }
}

// =========================
// STARTUP ANIMATION (SCANLINE)
// =========================
void startupAnim(){

  const char* msg = " FAT NINJA ";
  int len = strlen(msg);

  // REVERSED SCROLL DIRECTION
  for(int offset = 10; offset > -len*4; offset--){

    clearAll();

    for(int i=0;i<len;i++){
      drawChar(msg[i], offset + i*4);
    }

    FastLED.show();
    delay(65);
  }

  // ignition flash
  for(int i=0;i<2;i++){
    fill_solid(r9,9,CRGB(255,0,0));
    fill_solid(r8,8,CRGB(255,0,0));
    fill_solid(r6,6,CRGB(255,0,0));
    fill_solid(l9,9,CRGB(255,0,0));
    fill_solid(l8,8,CRGB(255,0,0));
    fill_solid(l6,6,CRGB(255,0,0));

    FastLED.show();
    delay(120);

    clearAll();
    FastLED.show();
    delay(120);
  }
}
// =========================
// SETUP
// =========================
void setup(){
  pinMode(BRAKE_PIN,INPUT);
  pinMode(LEFT_PIN,INPUT);
  pinMode(RIGHT_PIN,INPUT);

  FastLED.addLeds<WS2812B,PIN_R9,GRB>(r9,9);
  FastLED.addLeds<WS2812B,PIN_R8,GRB>(r8,8);
  FastLED.addLeds<WS2812B,PIN_R6,GRB>(r6,6);

  FastLED.addLeds<WS2812B,PIN_L9,GRB>(l9,9);
  FastLED.addLeds<WS2812B,PIN_L8,GRB>(l8,8);
  FastLED.addLeds<WS2812B,PIN_L6,GRB>(l6,6);

  FastLED.clear();
  FastLED.show();

  startupAnim();
}

// =========================
// LOOP (EMPTY BASE)
// =========================
void loop(){
  // placeholder for brake / turn logic
}
