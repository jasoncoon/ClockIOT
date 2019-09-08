#include <Time.h>
#include <Wire.h>
#include <FastLED.h>

//#define ULTIM8x16 // DullesKlok
#define CLOCKIOT
#include <MatrixMaps.h>

#include "klok.h"
#include "textures.h"
#include "logic.h"
#include "Faceplate.h"


#define BAREBONES
#ifndef BAREBONES
#include <WiFiManager.h>
#include "get_time.h"
#endif

#include "dutch_v1.h"
#include "french_v1.h"
#include "german_v3.h"
#include "hebrew_v1.h"
#include "hungarian_v2.h"
#include "irish_v1.h"
#include "italian_v1.h"
#include "english_v3.h"
#include "spanish_v1.h"

#include "config.h"

#ifndef BAREBONES
WiFiManager wifiManager;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "us.pool.ntp.org", 0, 60000);
NTPClock ntp_clock;
#endif

// DS3231Clock ds3231_clock;

// How many leds are in the strip?
const uint8_t N_BOARD = 2;
const uint8_t NUM_LEDS = 64 * N_BOARD;
bool mask[NUM_LEDS];
bool wipe[NUM_LEDS];
CRGB leds[NUM_LEDS];

#ifdef CLOCKIOT
#define DATA_PIN     4
#define CLK_PIN      16
#else
#define DATA_PIN     MOSI
#define CLK_PIN      SCK
#endif

#define COLOR_ORDER BGR
#define LED_TYPE APA102
#define MILLI_AMPS 1000  // IMPORTANT: set the max milli-Amps of your power supply (4A = 4000mA)

uint32_t last_time;

//********************************************************************************
// Displays
typedef void (*Init)();
typedef void (*DisplayTime)(uint32_t last_tm, uint32_t tm);

Faceplate faceplates[] = {
  english_v3,
  spanish_v1
};
uint8_t num_faceplates = 2;
uint8_t faceplate_idx = 0;

Klok klok(faceplates[0]);

void setPixelMask(bool* mask, uint8_t row, uint8_t col, bool b){
  if(row >= MatrixHeight){
  }
  else if(col >= MatrixWidth){
  }
  else{
    uint16_t pos = XY(col, row);
    if(pos < NUM_LEDS){
      mask[pos] = b;
    }
  }
}

void wipe_around(bool val){
  float dtheta = 31.4 / 180;
  float theta = -3.14 - dtheta;
  int row, col;
  bool tmp[NUM_LEDS];

  int cx = random(0, MatrixWidth-1);
  int cy = random(0, MatrixHeight-1);
  cx = 8;
  cy = 4;
  
  fillMask(wipe, !val);
  while (theta < 3.14 + dtheta){
    for(row=0; row < MatrixHeight; row++){
      for(col=0; col < MatrixWidth; col++){
	if(atan2(row - cy, col - cx) < theta){
	  setPixelMask(wipe, row, col, val);
	}
      }
    }
    logical_or(NUM_LEDS, wipe, mask, tmp);
    //rainbow_slow();
    apply_mask(tmp);
    FastLED.show();
    theta += dtheta;
    delay(10);
  }
  
}

typedef struct{
  Init       init;           // called when display changes
  DisplayTime display_time;  // called in main loop to update time display (if needed)
  String     name;
  int        id;
} Display;

void Plain_init(){
  uint32_t current_time = Now();
  last_time = current_time;
  blend_to_rainbow();
}
void Plain_display_time(uint32_t last_tm, uint32_t tm){
  fillMask(mask, false);
  faceplates[faceplate_idx].maskTime(tm, mask);
  rainbow_fast();
  apply_mask(mask);
}

Display *CurrentDisplay_p;
Display PlainDisplay = {Plain_init, Plain_display_time, String("Plain"), 0};
Display TheMatrixDisplay = {TheMatrix_init, TheMatrix_display_time, String("The Matrix"), 1};
Display WordDropDisplay = {WordDrop_init, WordDrop_display_time, String("Word Drop"), 2};

const uint8_t N_DISPLAY = 3;
Display Displays[N_DISPLAY] = {PlainDisplay, WordDropDisplay, TheMatrixDisplay};

//--------------------------------------------------------------------------------
//uint32_t current_time;
void blend_to_rainbow(){
  int i;
  CHSV newcolor;
  uint32_t current_time = Now();
  //current_time = Now();
  int count = ((current_time % 300) * 255) / 300;
  
  newcolor.val = 255;
  newcolor.sat = 255;
  for(int ii=0; ii<NUM_LEDS; ii++){
    for( int row = 0; row < MatrixHeight; row++) {
      for( int col = 0; col < MatrixWidth; col++) {
	i = XY(col, row);
	if(mask[i]){
	  newcolor.hue =  (count + (MatrixWidth * row + col) * 2) % 256;
	  nblend(leds[XY(col, row)], newcolor, 1);
	}
      }
    }
    FastLED.show();
    delay(1);
  }
}

void blend_to_color(CRGB color){
  for(int kk=0; kk<128; kk++){
    for(int ii=0; ii<NUM_LEDS; ii++){
      if(mask[ii]){
	nblend(leds[ii], color, 1);
      }
    }
    FastLED.show();
    delay(1);
  }
}

void blend_to_red(){
  blend_to_color(CRGB::Red);
}

void blend_to_green(){
  blend_to_color(CRGB::Green);
}

void blend_to_blue(){
  blend_to_color(CRGB::Blue);
}

void fill_red(){
  fill_solid(leds, NUM_LEDS, CRGB::Red);
}
void fill_green(){
  fill_solid(leds, NUM_LEDS, CRGB::Green);
}
void fill_blue(){
  fill_solid(leds, NUM_LEDS, CRGB::Blue);
}
void noop(){
}

void setWordMask(bool *mask, uint8_t* word, bool b){
  // word = [row, col, len]
  for(int i=0; i < word[2]; i++){
    setPixelMask(mask, word[1], word[0] + i, b);
  }
}


void WordDrop_init(){
  uint32_t current_time = Now();
  last_time = current_time;
  fillMask(mask, false);
  faceplates[faceplate_idx].maskTime(current_time, mask);
  blend_to_rainbow();
}

void word_drop_in(uint16_t time_inc){
  uint8_t bits;     // holds the on off state for 8 words at a time
  uint8_t word[3];  // start columm, start row, length of the current word
  bool tmp_mask[NUM_LEDS];
  uint8_t tmp_word[3];
  uint8_t n_byte_per_display = faceplates[faceplate_idx].displays[0];
  
  fillMask(mask, false);
  fillMask(wipe, false);
  fillMask(tmp_mask, false);
  
  for(uint8_t j = 0; j < n_byte_per_display; j++){ // j is a byte index 
    // read the state for the next set of 8 words
    bits = pgm_read_byte(faceplates[faceplate_idx].displays + 1 + (time_inc * n_byte_per_display) + j);
    for(uint8_t k = 0; k < 8; k++){                     // k is a bit index
      if((bits >> k) & 1){                              // check to see if word is on or off
	faceplates[faceplate_idx].getword(j * 8 + k, word);                       // if on, read location and length
	tmp_word[0] = word[0];
	tmp_word[1] = word[1];
	tmp_word[2] = word[2];
	for(int rr = 0; rr <= word[1]; rr++){
	  tmp_word[1] = rr;
	  setWordMask(wipe, tmp_word, true);
	  logical_or(NUM_LEDS, mask, wipe, tmp_mask);
	  rainbow_slow();
	  apply_mask(tmp_mask);
	  FastLED.show();
	  delay(25);
	}
	setWordMask(mask, word, true);
	for(int rr = 0; rr < word[1]; rr++){
	  tmp_word[1] = rr;
	  setWordMask(wipe, tmp_word, false);
	  logical_or(NUM_LEDS, mask, wipe, tmp_mask);
	  
	  rainbow_slow();
	  apply_mask(tmp_mask);
	  FastLED.show();
	  delay(25);
	}
      }
    }
  }
}
void word_drop_out(uint16_t time_inc){
  uint8_t bits;     // holds the on off state for 8 words at a time
  uint8_t word[3];  // start columm, start row, length of the current word
  bool tmp_mask[NUM_LEDS];
  uint8_t tmp_word[3];
  uint8_t n_byte_per_display = faceplates[faceplate_idx].displays[0];
  
  //fillMask(mask, false);
  //fillMask(wipe, false);
  //fillMask(tmp_mask, false);
  logical_copy(NUM_LEDS, mask, wipe);
  logical_copy(NUM_LEDS, mask, tmp_mask);
  
  for(uint8_t j = 0; j < n_byte_per_display; j++){ // j is a byte index 
    // read the state for the next set of 8 words
    bits = pgm_read_byte(faceplates[faceplate_idx].displays + 1 + (time_inc * n_byte_per_display) + j);
    for(uint8_t k = 0; k < 8; k++){                     // k is a bit index
      if((bits >> k) & 1){                              // check to see if word is on or off
	faceplates[faceplate_idx].getword(j * 8 + k, word);                       // if on, read location and length
	tmp_word[0] = word[0];
	tmp_word[1] = word[1];
	tmp_word[2] = word[2];
	for(int rr = word[1]; rr <= 8; rr++){
	  tmp_word[1] = rr;
	  setWordMask(wipe, tmp_word, true);
	  logical_or(NUM_LEDS, mask, wipe, tmp_mask);
	  rainbow_slow();
	  apply_mask(tmp_mask);
	  FastLED.show();
	  delay(25);
	}
	setWordMask(mask, word, false);
	for(int rr = word[1]; rr <= 8; rr++){
	  tmp_word[1] = rr;
	  setWordMask(wipe, tmp_word, false);
	  logical_or(NUM_LEDS, mask, wipe, tmp_mask);
	  
	  rainbow_slow();
	  apply_mask(tmp_mask);
	  FastLED.show();
	  delay(25);
	}
      }
    }
  }
}

void word_drop(uint16_t last_time_inc, uint16_t time_inc){
  bool tmp_d[NUM_LEDS];

  rainbow_slow();

  // swipe rainbow from the left
  //wipe_around(true);
  //delay(1000);
  if(last_time_inc != 289){
    word_drop_out(last_time_inc);
  }
  
  // clear the new display
  fillMask(tmp_d, false);
  
  // read display for next time incement
  faceplates[faceplate_idx].maskTime(time_inc * 300, mask);
  
  // clear rainbow to reveal the time
  //wipe_off_left();
  //wipe_around(false);
  word_drop_in(time_inc);
}

void WordDrop_display_time(uint32_t last_tm, uint32_t next_tm){
  int last_tm_inc = (last_tm / 300) % 288;
  int      tm_inc = (next_tm / 300) % 288;
  if(last_tm_inc == tm_inc - 1 || (last_tm_inc == 287 && tm_inc == 0)){
    word_drop(last_tm_inc, tm_inc);
  }
  else if(last_tm_inc != tm_inc){
    rainbow_slow();
    fillMask(mask, false);
    faceplates[faceplate_idx].maskTime(next_tm, mask);  
    apply_mask(mask);
  }
}

void TheMatrix_drop(uint32_t last_tm_inc, uint32_t current_tm_inc){
  int n_drop = 0;
  int n_need = 8;
  
  const struct CRGB color = CRGB::Green;
  uint8_t cols[NUM_LEDS];
  uint8_t rows[NUM_LEDS];
  uint8_t pause[NUM_LEDS];
  bool have[NUM_LEDS];
  int col;
  int i, j;

  Serial.print("TheMatrix: Change Time\n");
  // clear all masks
  fillMask(mask, false);
  fillMask(wipe, false);
  fillMask(have, false);

  // set masks to appropriate times

  faceplates[faceplate_idx].maskTime(last_tm_inc * 300, mask);
  faceplates[faceplate_idx].maskTime(current_tm_inc * 300, wipe);
  fill_green();
  apply_mask(mask);
  FastLED.show();
    
  for(i=0; i < MatrixWidth; i++){
    for(j=0; j < MatrixHeight; j++){
      if(leds[XY(i, j)].red > 0 ||
	 leds[XY(i, j)].green > 0 ||
	 leds[XY(i, j)].blue > 0){
	rows[n_drop] = j;
	cols[n_drop] = i;
	n_drop++;
      }
      if(wipe[XY(i, j)]){
	n_need++;
      }
    }
  }
  
  delay(10);
  for(j = 0; j < 255 * 3; j++){
    for(i=0; i < NUM_LEDS; i++){
      leds[i].red   = blend8(leds[i].red, 0, 1);
      leds[i].green = blend8(leds[i].green, 255, 1);
      leds[i].blue  = blend8(leds[i].blue, 0, 1);
    }
    apply_mask(mask);
    FastLED.show();
    delay(5);
  }

  for(i = n_drop; i < n_need; i++){/// add enough drops to complete
    cols[i] = random(0, MatrixWidth);
    rows[i] = -random(0, MatrixHeight);
    n_drop++;
  }

  int end = millis() + 5000; // go for 5 seconds
  // while new display is not filled out
  while(!logical_equal(NUM_LEDS, wipe, have)){
    //  while(millis() < end){
    fadeToBlackBy(leds, NUM_LEDS, 75);
    for(i = 0; i < n_drop; i++){
      if(millis() > end && wipe[XY(cols[i], rows[i])]){
	if(random(0, 3) == 0){
	  have[XY(cols[i], rows[i])] = true;
	}
      }
      
      if(random(0, 16) == 0){ // pause at random times
	pause[i] = random(6, 9); // for random duration
      }
      if(pause[i] == 0){
	rows[i]++;
      }
      else{
	pause[i]--; 
      }
      if(rows[i] > MatrixHeight - 1){
	if(n_drop > n_need){
	  for(j = i; j < n_drop; j++){ // slide drops down by one
	    rows[j] = rows[j + 1];
	    cols[j] = cols[j + 1];
	  }
	  n_drop--;
	  Serial.print("n_drop:");
	  Serial.println(n_drop);
	}
	else{
	  rows[i] = -random(0, MatrixHeight);
	  cols[i] = random(0, MatrixWidth);
	}
      }
      if(0 <= rows[i] && rows[i] <  MatrixHeight){
	leds[XY(cols[i], rows[i])] = color;
      }
    }

    for(int ii = 0; ii < NUM_LEDS; ii++){
      if(have[ii]){
	leds[ii] = CRGB::Blue;
      }
    }
    FastLED.show();
    delay(75);
  }
  for(int ii=0; ii< MatrixHeight * 10; ii++){
    //  while(millis() < end){
    fadeToBlackBy(leds, NUM_LEDS, 75);
    for(i = 0; i < n_drop; i++){
      rows[i]++;
      if(0 <= rows[i] && rows[i] <  MatrixHeight){
	leds[XY(cols[i], rows[i])] = color;
      }
    }
    for(int ii = 0; ii < NUM_LEDS; ii++){
      if(have[ii]){
	leds[ii] = CRGB::Blue;
      }
    }
    FastLED.show();
    delay(75);
  }
}

void TheMatrix_init(){
  uint32_t current_time = Now();
  last_time = current_time;
  blend_to_blue();
  fill_blue();
  fillMask(mask, false);
  faceplates[faceplate_idx].maskTime(current_time, mask);  
  apply_mask(mask);
}

void TheMatrix_display_time(uint32_t last_tm, uint32_t tm){

  int last_tm_inc = (last_tm / 300) % 288;
  int      tm_inc = (     tm / 300) % 288;
  
  if(last_tm_inc == tm_inc - 1 || (last_tm_inc == 287 && tm_inc == 0)){
    TheMatrix_drop(last_tm_inc, tm_inc);
  }
  else if(last_tm_inc != tm_inc){
    fill_blue();
    fillMask(mask, false);
    faceplates[faceplate_idx].maskTime(tm, mask);  
    apply_mask(mask);
    apply_mask(mask);
  }
}

void rainbow_cycle(int count){
  int i, dx, dy;
  CHSV hsv;
  float dist;
  
  hsv.hue = 0;
  hsv.val = 255;
  hsv.sat = 240;

  for( int row = 0; row < MatrixHeight; row++) {
    for( int col = 0; col < MatrixWidth; col++) {
      // dx, dy, dist used for radial pattern, not used here
      dy = (row - 4) * 2;
      dx = col - 8;
      dist = sqrt(dx * dx + dy * dy);
      i = XY(col, row);
      //hsv.hue =  ((int)(dist * 16) - count) % 256;
      hsv.hue =  (count + (MatrixWidth * row + col) * 2) % 256;
      leds[i] = hsv;
    }
  }
}

void rainbow_fast() {
  uint32_t current_time = Now();
  int count = millis() / 100;
  
  rainbow_cycle(millis()/25);
  // Show the leds (only one of which is set to white, from above)
  //delay(100);
}

void rainbow_slow() {
  uint32_t current_time = Now();
  int count = ((current_time % 300) * 255) / 300;
  rainbow_cycle(count);
}

// end Displays
//********************************************************************************

uint8_t logo_rgb[] = {
  0x11,0x00,0x29,0x00,0x25,0x00,0x23,0x00,0x25,0x00,0x29,0x00,0x31,0x00,0xe0,0x01,
  0x00,0x03,0x80,0x04,0x80,0x04,0x80,0x04,0x80,0x04,0x00,0x03,0x00,0x00,0x00,0x00,
  0x11,0x00,0x09,0x88,0x05,0x48,0x03,0x28,0x05,0x18,0x09,0x28,0x11,0x48,0x00,0x88
};

struct config_t{
  int timezone;
  long alarm;
  int mode;
  uint8_t brightness;
  uint8_t display_idx;
  bool summer_time;
} configuration;

void ChangeDisplay(Display* display_p);
void ChangeDisplay(Display* display_p){
  CurrentDisplay_p = display_p;
  CurrentDisplay_p->init();
}

// Common Interface for buttons and MQTT
void set_brightness(uint8_t brightness){
  if(brightness < 256){
    configuration.brightness = brightness;
    FastLED.setBrightness(configuration.brightness);
    Serial.println(configuration.brightness);
    saveSettings();
  }
}

void adjust_brightness(int delta){
  int new_val = delta + configuration.brightness;
  if(0 < new_val && new_val < 256){
    set_brightness(new_val);
  }
}

void set_display(uint8_t display_idx){
  configuration.display_idx = display_idx % N_DISPLAY;
  ChangeDisplay(&Displays[display_idx % N_DISPLAY]);
  saveSettings();
}
void next_display(){
  configuration.display_idx = (configuration.display_idx + 1) % N_DISPLAY;
  ChangeDisplay(&Displays[configuration.display_idx]);
  saveSettings();
}

void add_to_timezone(int32_t offset){ // does not include summer time offset
  configuration.timezone += offset;
  saveSettings();
}

void set_timezone_offset(int32_t offset){ // does not include summer time offset
  configuration.timezone = offset % 86400;
  saveSettings();
}

void set_summer_time(bool summer_time){                   // AKA Dailight Savings Time
  configuration.summer_time = summer_time;
  saveSettings();
}

void toggle_summer_time(){
  configuration.summer_time = !configuration.summer_time;
  saveSettings();  
}

void display_bitmap_rgb(uint8_t* bitmap){
  uint8_t n = 16;
  uint8_t h = 8;
  uint8_t w = 16;
  uint8_t xy[2];
  uint8_t r, g, b;
  
  int i, j;
  int led_idx, byte_idx;

  //struct CRGB color;
  int x, y;
  
  for(i=0; i<n; i++){
    r = bitmap[i + 0 * w];
    g = bitmap[i + 1 * w];
    b = bitmap[i + 2 * w];
    //   012345678pabcdef
    // 0 0000000011111111
    // 1 2222222233333333
    // 2 4444444455555555
    // 3 6666666677777777
    // 4 8888888899999999
    // 5 aaaaaaaabbbbbbbb
    // 6 ccccccccdddddddd
    // 7 eeeeeeeeffffffff
    for(j=0; j<8; j++){
      x = (i * 8 + j) % 16;
      y = i / 2;
      led_idx = XY(x, y);
      leds[led_idx].red   = 255 * ((r >> j) & 1);
      leds[led_idx].green = 255 * ((g >> j) & 1);
      leds[led_idx].blue  = 255 * ((b >> j) & 1);
      if(leds[led_idx].red || leds[led_idx].green || leds[led_idx].blue){
	mask[led_idx] = true;
      }
    }
  }
}

void apply_mask(bool* mask){
  uint16_t b, k;
  for(uint16_t i=0; i < NUM_LEDS; i++){
    if(!mask[i]){
      leds[i] = CRGB::Black;
    }
  }
}

void fillMask(bool* mask, bool b){
  fillMask(mask, b, 0, NUM_LEDS);
}

void fillMask(bool* mask, bool b, int start, int stop){
  for(int i = start; i < stop && i < NUM_LEDS; i++){
    mask[i] = b;
  }
}


void loadSettings(){
}

void saveSettings(){
}

uint16_t XY( uint8_t x, uint8_t y){
  uint16_t out = 0;
  if(FLIP_DISPLAY){
    x = MatrixWidth - x - 1;
    y = MatrixHeight - y - 1;
  }
  if(x < MatrixWidth && y < MatrixHeight){
    out = MatrixMap[y][x];
  }
  return out;
}

void led_setup(){
  FastLED.addLeds<LED_TYPE, DATA_PIN, CLK_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setDither(true);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MILLI_AMPS);
  fill_solid(leds, NUM_LEDS, CRGB::Red);
  FastLED.show();
  delay(1000);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

/*********************************************************************************/
// Button stuff
void button_setup(){
}
// Button stuff
/*********************************************************************************/

/*********************************************************************************/
// wifi setup
#ifndef BAREBONES
void wifi_setup(){
  wifiManager.autoConnect("KLOK");
  Serial.println("Yay connected!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
#endif
// wifi setup
/*********************************************************************************/

void setup(){
  last_time = 0;
  
  CurrentDisplay_p = &PlainDisplay;

  Wire.begin();
  Serial.begin(115200);
  delay(200);
  Serial.println("setup() starting");
  //Serial.println(year(0));
  led_setup();
  CurrentDisplay_p = &Displays[configuration.display_idx % N_DISPLAY];
  
  for(int ii = 0; ii < num_faceplates; ii++){
    faceplates[ii].setup(MatrixWidth, MatrixHeight, XY);
  }


  fill_solid(leds, NUM_LEDS, CRGB::Red);
  FastLED.show();
  delay(100);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  delay(900);

#ifndef BAREBONES
  wifi_setup();
  ntp_clock.setup(&timeClient);
  int mumbai_offset = 19800;
  int dc_offset = -14400;
  ntp_clock.setOffset(mumbai_offset);
  //ntp_clock.setOffset(dc_offset);
#endif
  Serial.println("setup() complete");
}

uint32_t Now(){
#ifndef BAREBONES
  return ntp_clock.now();
#else
  //  return ds3231_clock.now();
  return millis()/1000;
#endif
}

void loop(){
  uint8_t word[3];
  uint32_t current_time = Now();
  
  fillMask(mask, true);
  CurrentDisplay_p->display_time(last_time, current_time);
  FastLED.show();

}
