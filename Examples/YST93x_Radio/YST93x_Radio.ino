/*

The YST93x (YST936, YST939, etc.) is an AM/FM tuner module used on some cheap
Chinese home theaters and sound systems. It is made by a company named Winever
(http://www.winever.cn/) - and maybe some others.  It is very cheap and of a
a reasonable quality.  It is easy to find on eBay and Alibaba.com

The module is based on a Sanyo LC72131 PLL and a tuner IC.  The tuner chip varies
from model to model: YST939 uses the Sanyo LA1823 (or clone) radio-on-chip and
YST936 uses a Chinese-branded "CS1000" tuner.

The PLL can communicate with a microcontroller via the Sanyo CCB bus, and have
5 I/O pins which controls the tuner:

PLL pin    Direction       Function
BO0        PLL -> Tuner    Not used
BO1        PLL -> Tuner    Band selector (0 = FM; 1 = AM)
BO2        PLL -> Tuner    Mute / IF output (0 = Mute / IF counter mode
                                             1 = Normal tuner mode)
BO3        PLL -> Tuner    Audio mode (0 = Stereo; 1 = Mono)
BO4        PLL -> Tuner    Not used
IO0        Tuner -> PLL    Not used (pulled high.  Reads "1")
IO1        Tuner -> PLL    Stereo indicator (0 = Stereo; 1 = Mono)

Note: BO0 must be set to "0" for the tuner to output the IF signal to the PLL.
This means the tuner will have to be muted every time one want the PLL to count
the IF frequency.

YST93x module Pinout:

  FM 
Antenna +------------------------+
    +---|                        |- RDS out (not all models have this pin)
    +---|                        |- Out-L
        |         YST93x         |- GND
        |                        |- Out-R 
        |                        |- VDD (12V)
        |                        |
  AM    |                        |
Antenna |                        |- DO
    +---|                        |- CL
    |   |                        |- DI
    |   |                        |- CE
    +---|                        |- GND
        +------------------------+
        
YST93x to Arduino connections:
Arduino                YST93x
A5 (DO)                  DI
A4 (CL)                  CL
A3 (DI)                  DO
A2 (CE)                  CE
GND                      GND

Note:   This example uses an analog keypad with the following schematics:

                 A0
                 |
          2k2    |   330R        620R         1k          3k3        
VCC -----\/\/\---+---\/\/\---+---\/\/\---+---\/\/\---+---\/\/\-----+----- GND
                 |           |           |           |             |
                 X SCAN_UP   X UP        X DOWN      X SCAN_DOWN   X BAND
                 |           |           |           |             |
                GND         GND         GND         GND           GND

X = keys (N.O.)

*/

#include <inttypes.h>
#include <LiquidCrystal.h>
#include <SanyoCCB.h>

// This example uses an analog 5-key keypad tied to A0
#define KEYPAD_PIN A0

// Keypad keys
#define KEY_BAND      5
#define KEY_SCAN_DOWN 4
#define KEY_DOWN      3
#define KEY_UP        2
#define KEY_SCAN_UP   1
#define KEY_NONE      0

// LC72131 IN1, byte 0
#define IN10_R3     7
#define IN10_R2     6
#define IN10_R1     5
#define IN10_R0     4
#define IN10_XS     3
#define IN10_CTE    2
#define IN10_DVS    1
#define IN10_SNS    0

// LC72131 IN2, byte 0
#define IN20_TEST2  7
#define IN20_TEST1  6
#define IN20_TEST0  5
#define IN20_IFS    4
#define IN20_DLC    3
#define IN20_TBC    2
#define IN20_GT1    1
#define IN20_GT0    0

// LC72131 IN2, byte 1
#define IN21_DZ1    7
#define IN21_DZ0    6
#define IN21_UL1    5
#define IN21_UL0    4
#define IN21_DOC2   3
#define IN21_DOC1   2
#define IN21_DOC0   1
#define IN21_DNC    0

// LC72131 IN2, byte 2
#define IN22_BO4    7
#define IN22_BO3    6
#define IN22_BO2    5
#define IN22_BO1    4
#define IN22_IO2    3
#define IN22_IO1    2
#define IN22_IOC2   1
#define IN22_IOC1   0

// LC72131 DO0, byte 0
#define DO0_IN2     7
#define DO0_IN1     6
#define DO0_UL      4

// For function YST93xSetMode
#define YST93x_MONO    1
#define YST93x_STEREO  2
#define YST93x_MUTE    3
#define YST93x_UNMUTE  4
#define YST93x_BAND_FM 5
#define YST93x_BAND_AM 6

// Acceptable IF frequency deviation window (for the PLL) when searching for radio stations
// You may need to tweek these values to have a reliable "scan" mode
#define FM_TUNED_WINDOW 180
#define AM_TUNED_WINDOW 20

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

SanyoCCB ccb(A5, A4, A3, A2); // Pins: DO CL DI CE

byte pll_in1[3];
byte pll_in2[3];

// Initial frequencies
uint16_t FMFrequency = 880;   // MHz * 10
uint16_t AMFrequency = 53;    // KHZ / 10

uint8_t band = YST93x_BAND_FM;
uint8_t tuned = 0;


/************************************************\
 *               YST93xInit()                   *
 * Initialize the PLL settings vectors with     *
 * parameters common to booth AM and FM modes   *
\************************************************/ 
void YST93xInit() {
  memset(pll_in1, 0, 3);
  memset(pll_in2, 0, 3);
  bitSet(pll_in2[0], IN20_IFS);   // IF counter in normal mode
  bitSet(pll_in2[1], IN21_UL0);   // Phase error detection width = 0us
  bitSet(pll_in2[2], IN22_BO2);   // Mute off / normal tuner mode
}


/************************************************\
 *              YST93xSetMode()                 *
 * Some predefined setups for the YST93x module *
\************************************************/
void YST93xSetMode(uint8_t mode) {
  switch(mode) {
    case YST93x_STEREO:
      bitClear(pll_in2[2], IN22_BO3);
      break;

    case YST93x_MONO:
      bitSet(pll_in2[2], IN22_BO3);
      break;

    case YST93x_MUTE:
      bitClear(pll_in2[2], IN22_BO2);
      break;

    case YST93x_UNMUTE:
      bitSet(pll_in2[2], IN22_BO2);
      break;

    case YST93x_BAND_FM:
      band = YST93x_BAND_FM;
      bitWrite(pll_in1[0], IN10_R0,  1); // Reference frequency = 50kHz
      bitWrite(pll_in1[0], IN10_R3,  0); //
      bitWrite(pll_in1[0], IN10_DVS, 1); // Programmable Divider divisor = 2
      bitWrite(pll_in2[0], IN20_GT0, 0); // IF counter mesurement period = 32ms
      bitWrite(pll_in2[0], IN20_GT1, 1); //
      bitWrite(pll_in2[1], IN21_DZ0, 1); // Dead zone = DZB
      bitWrite(pll_in2[1], IN21_DZ1, 0); //
      bitWrite(pll_in2[2], IN22_BO1, 0); // FM mode
      break;
    
    case YST93x_BAND_AM:
      band = YST93x_BAND_AM;
      bitWrite(pll_in1[0], IN10_R0,  0); // Reference frequency = 10kHz
      bitWrite(pll_in1[0], IN10_R3,  1); //
      bitWrite(pll_in1[0], IN10_DVS, 0); // Programmable Divider divisor = 1
      bitWrite(pll_in2[0], IN20_GT0, 1); // IF counter mesurement period = 8ms
      bitWrite(pll_in2[0], IN20_GT1, 0); //
      bitWrite(pll_in2[1], IN21_DZ0, 0); // Dead zone = DZC
      bitWrite(pll_in2[1], IN21_DZ1, 1); //
      bitWrite(pll_in2[2], IN22_BO1, 1); // AM mode
      break;
  }
  ccb.write(0x82, pll_in1, 3); 
  ccb.write(0x92, pll_in2, 3); 
}



/************************************************************\
 *                       YST93xTune()                       *
 * Set the tuner frequency and return 1 if it is tuned      *
 * or 0 otherwise.                                          *
 *                                                          *
 * The frequency divisors was chosen in a way the frequency *
 * representation can be directly sent to the PLL and is    *
 * easy to represent:                                       *
 * - FM mode (divisor = 100): frequency (MHz) * 10          *
 * - AM mode (divisor = 10):  frequency (kHZ) / 10          *
\************************************************************/
uint8_t YST93xTune(uint16_t frequency) {
  uint16_t fpd = 0;
  uint8_t i = 0;
  uint8_t r[3];
  unsigned long IFCounter = 0;
  
  switch(band) {
    case YST93x_BAND_FM:
      // FM: fpd = (frequency + FI) / (50 * 2)
      fpd = (frequency + 107);
      break;
      
    case YST93x_BAND_AM:
      // AM: fpd = ((frequency + FI) / 10) << 4
      fpd = (frequency + 45) << 4;
      break;
      
    default: return 1;
  }

  YST93xSetMode(YST93x_MUTE);   // YST93x only injects FI signal into the PLL when in MUTE mode

  // Reset the IF counter and program the Frequency Programmable Divider (fpd)
  bitClear(pll_in1[0], IN10_CTE);
  pll_in1[1] = byte(fpd >> 8);
  pll_in1[2] = byte(fpd & 0x00ff);
  ccb.write(0x82, pll_in1, 3);

  // Start the IF counter
  bitSet(pll_in1[0], IN10_CTE);
  ccb.write(0x82, pll_in1, 3);

  // Wait for PLL to be locked (DO0_UL == 1)
  while(i < 20) {
    delay(50);
    ccb.read(0xa2, r, 3);  // Discard the 1st result: it is latched from the last count (as said on the datasheet)
    ccb.read(0xa2, r, 3);  // The 20 rightmost bits from r[0..2] are the IF counter result
    i = (bitRead(r[0], DO0_UL)) ? 100 : i + 1;
  }
  
  YST93xSetMode(YST93x_UNMUTE);   // Mute off / normal tuner mode

  if(i == 100) {
    // PLL is locked.  If the IF deviation is within the defined (window) interval,
    // the radio is likely to be tuned.
    // Note: this "tuned" detection method is not recommended on the LC72131 datasheet as 
    // it can give false positive results.  A better approach would be to get the "tuned"
    // flag from a RDS decoder with signal quality detection (e.g.: PT2579 or Philips SAA6588)
    // connected to the YST93x tuner module "RDS Output" pin. SAA6588 is more powerful/popular, 
    // but PT2579 looks a lot easier to use as it is not programmable and has a dedicated
    // signal quality output pin.
    IFCounter = (r[0] & 0x0f);
    IFCounter = (IFCounter << 16) | (unsigned long)(r[1] << 8) | (r[2]);
    Serial.println(IFCounter);
    switch(band) {
      case YST93x_BAND_FM:
        // Expected value: IF (10.7MHz) * Mesurement period (32ms, as set via GT[1..0]) = 342400
        if((IFCounter > 342400) && ((IFCounter - 342400) < FM_TUNED_WINDOW)) return 1;
        if((IFCounter < 342400) && ((342400 - IFCounter) < FM_TUNED_WINDOW)) return 1;
        break;
      case YST93x_BAND_AM:
        // Expected value: IF (450kHz) * Mesurement period (8ms, as set via GT[1..0]) = 3600
        // Note: scan mode in AM is really poor.  I have done my best in tweaking it, but it barely works
        if((IFCounter > 3600) && ((IFCounter - 3600) < AM_TUNED_WINDOW)) return 1;
        if((IFCounter < 3600) && ((3600 - IFCounter) < AM_TUNED_WINDOW)) return 1;
        break;
    }
  }
  return 0; 
}


/**************************************************\
 *                   YST93xIsStereo()             *
 * Returns 1 if the tuned radio station is stereo *
\**************************************************/
uint8_t YST93xIsStereo() {
  uint8_t r[3];
  
  ccb.read(0xa2, r, 3);
  return(bitRead(r[0], DO0_IN2) ? 0 : 1);
}


/*******************************************************\
 *                       getKey()                      *
 * Read the (analog) keypad.                           *
 * If you are using an digital (one key per input pin) *
 * keypad, just this function to return the correct    *
 * values                                              *
\*******************************************************/
uint8_t getKey(uint8_t keypadPin) {
  uint16_t keypad;
  uint8_t key = KEY_NONE;
  
  keypad = analogRead(keypadPin);
  
  if(keypad < 870) key = KEY_BAND;  
  if(keypad < 600) key = KEY_SCAN_DOWN;  
  if(keypad < 390) key = KEY_DOWN;  
  if(keypad < 220) key = KEY_UP;  
  if(keypad < 60)  key = KEY_SCAN_UP;
  
  return key;
}


/*******************\
 * Arduino setup() *
\*******************/ 
void setup() {
  lcd.begin(16, 2);
  Serial.begin(9600);
  ccb.init();
  delay(1000);
  YST93xInit();
  YST93xSetMode(YST93x_BAND_FM);
  tuned = YST93xTune(FMFrequency);
}


/******************\
 * Arduino loop() *
\******************/ 
void loop() {
  uint8_t scan = 0;
  int8_t delta = 0;
  uint8_t key = getKey(KEYPAD_PIN);

  lcd.setCursor(0, 0);
  switch(band) {
    case YST93x_BAND_FM:
      lcd.print("FM ");
      lcd.setCursor((FMFrequency < 1000) ? 4 : 3, 0); lcd.print((float)FMFrequency / 10, 1);
      lcd.print("MHz ");
      break;
    case YST93x_BAND_AM:
      lcd.print("AM ");
      lcd.setCursor((AMFrequency < 100) ? 4 : 3, 0); lcd.print(AMFrequency * 10);
      lcd.print("KHz ");
      break;
  }
  
  // Updates the Stereo indicator
  lcd.setCursor(12, 0);
  if(YST93xIsStereo())
    lcd.print("[ST]");
  else
    lcd.print("[  ]");

  // The "Tuned" indicator is updated only when the station changes
  lcd.setCursor(2, 1);
  if(tuned)    
    lcd.print("   Tuned   ");
  else
    lcd.print("           ");

  // Processo keypad inputs
  if(key != KEY_NONE) {  
    switch(key) {
      case KEY_UP:        scan = 0; delta = +1; break;
      case KEY_DOWN:      scan = 0; delta = -1; break;
      case KEY_SCAN_UP:   scan = 1; delta = +1; break;
      case KEY_SCAN_DOWN: scan = 1; delta = -1; break;
      case KEY_BAND:
        switch(band) {
          case YST93x_BAND_FM:
            YST93xSetMode(YST93x_BAND_AM);
            tuned = YST93xTune(AMFrequency);
            break;

          case YST93x_BAND_AM:
            YST93xSetMode(YST93x_BAND_FM);
            tuned = YST93xTune(FMFrequency);
            break;
        }
    }

    if(scan) {
      lcd.setCursor(2, 1);
      lcd.print("Scanning...");
    }

    // Change the station  
    switch(band) {
      case YST93x_BAND_FM:
        do{
          FMFrequency += delta;
          if(FMFrequency > 1080) FMFrequency = 880;
          if(FMFrequency < 880)  FMFrequency = 1080;
          tuned = YST93xTune(FMFrequency);

          lcd.setCursor(3, 0);
          if(FMFrequency < 1000) lcd.print(' ');
          lcd.print((float)FMFrequency / 10, 1);
        } while(scan && (! tuned));
        break;
        
      case YST93x_BAND_AM:
        do{
          AMFrequency += delta;
          if(AMFrequency > 170) AMFrequency = 53;
          if(AMFrequency < 53)  AMFrequency = 170;
          tuned = YST93xTune(AMFrequency);

          lcd.setCursor(3, 0);
          if(AMFrequency < 100) lcd.print(' ');
          lcd.print(AMFrequency * 10);
        } while(scan && (! tuned));
        break;
        
    } // switch
    
  } // if(key)
  
}
  
