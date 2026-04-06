#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include "LiquidCrystal_I2C.h"
#include "SoftPathElectronics.h"
//#include "field.h"

#define MaxHight 150

#define KEY_1 1
#define KEY_2 2
#define KEY_3 3
#define KEY_4 5
#define KEY_5 6
#define KEY_6 7
#define KEY_7 9
#define KEY_8 10
#define KEY_9 11

#define KEY_0 14
#define KEY_DOT 13
#define KEY_BACK 15
#define KEY_STOP 16

#define KEY_MODE1 4
#define KEY_MODE2 8
#define KEY_MODE3 12



extern float BendAngle;
extern float CorrectionAngle;
extern float BendLength;
extern float MaterialThickness;
extern float Retract;
extern float MatrixWidth;
extern float MatrixHeight;
extern float MatrixRadius;
extern float StampHeight;
extern float FreeSpace;

extern float CutLength;

extern float Z_Goalpos;
extern float X_Goalpos;
extern float Y_Goalpos;

extern bool Z_homed;
extern bool X_homed;
extern bool Y_homed;

extern uint8_t Mode;
extern uint8_t lastMode;
extern uint8_t HomingMode;
extern uint8_t GlobalMode;


extern unsigned long GlobalPos;

struct Field {
  uint8_t colonCol;
  uint8_t colonRow;
  uint8_t valCol;
  uint8_t valRow;
  uint8_t width;
  uint8_t decimals;
  float   minVal;
  float   maxVal;
  float*  value;
};

struct HomingItem {
    uint8_t col;        // where the selectable item is drawn
    uint8_t row;
    const char* text;   // "-", "Z", "+", etc.
    uint8_t modeValue;  // HomingMode to set when selected
};

// Public functions
void Display();
void DisplayInit();
float Zpos2mm();
void handleModeKey(uint8_t targetMode);
void drawHomingLine();
void drawMode3Page();
bool StopAction();

#endif
