/*
Author: Yannick Wunderle
Date: 04.04.2026
Description: Code to run a finger press brake with 2 axis endstop and shears
ToDo: 
Software:
Ramp up (and down) to reach higher velocity
Read in keyboard
Display
Allow key interrupt (cancel/switch mode)

Electrical:
Connect display
Connect keyboard
Mound components
Pull up z limit switch
junction box
Mod Z2 and X axis

11cm in 33s with Homing (500uS steptime), 100steps/rev -->perfect match, 1200rpm (but conf0.cmd_res = 200)
*/
#include "DisplayManager.h"


#define Light_Pin 47
#define DownholderUp_Pin 53
#define DownholderL_Pin 49
#define DownholderR_Pin 51
#define ShuntR_Pin A14
#define ShuntL_Pin A15

#define Keyboard_Pin A8  //unused. Has to be hardcoded in lib
#define Estop_Pin A9
#define SwitchFoot_Pin 5
#define Precharge_Pin 45
#define SwitchZ_Pin 4
#define FuEnble_Pin A13
#define DirH_Pin A4
#define DirL_Pin A5
#define StepH_Pin A6
#define StepL_Pin A7

#define EndstopL_Pin 13
#define EndstopH_Pin 10
#define EndstopEnX_Pin 12
#define EndstopEnZ_Pin 11
#define EndstopTachZ_Pin 2
#define EndstopTachX_Pin 3
#define EndstopSwitchZ_Pin 8
#define EndstopSwitchX_Pin 9

#define PrechargeTime 2000
#define MaxHight 150
#define UpperToolHeight 165.7

#define CutHightUp 60
#define CutHightDown 2

bool debug = true;

unsigned long PrechargeTimer = 0;
unsigned long GlobalPos = 0;
bool precharged = false;

float BendAngle = 90;
float CorrectionAngle = 0;
float BendLength = 100;
float MaterialThickness = 1.25;
float Retract = 0;
float MatrixWidth = 12;
float MatrixHeight = 75;
float MatrixRadius = 0.1;
float StampHeight = 110;
float FreeSpace = 20;

float CutLength = 120;

float Z_Goalpos = 60;
float X_Goalpos = 100;
float Y_Goalpos = 150;

bool Z_homed = false;
bool X_homed = false;
bool Y_homed = false;

uint8_t Mode = 3;
uint8_t lastMode = 0;
uint8_t HomingMode = 0;
uint8_t GlobalMode = 0;

void setup() {
  Serial.begin(9600);

  pinMode(Light_Pin, OUTPUT);
  pinMode(DownholderUp_Pin, OUTPUT);
  pinMode(DownholderL_Pin, OUTPUT);
  pinMode(DownholderR_Pin, OUTPUT);
  pinMode(Precharge_Pin, OUTPUT);
  pinMode(FuEnble_Pin, OUTPUT);
  pinMode(DirH_Pin, OUTPUT);
  pinMode(DirL_Pin, OUTPUT);
  pinMode(StepH_Pin, OUTPUT);
  pinMode(StepL_Pin, OUTPUT);
  pinMode(EndstopL_Pin, OUTPUT);
  pinMode(EndstopH_Pin, OUTPUT);
  pinMode(EndstopEnX_Pin, OUTPUT);
  pinMode(EndstopEnZ_Pin, OUTPUT);

  pinMode(ShuntR_Pin, INPUT);
  pinMode(ShuntL_Pin, INPUT);
  pinMode(Estop_Pin, INPUT);
  pinMode(SwitchFoot_Pin, INPUT);
  pinMode(SwitchZ_Pin, INPUT);
  pinMode(EndstopTachZ_Pin, INPUT);
  pinMode(EndstopTachX_Pin, INPUT);
  pinMode(EndstopSwitchZ_Pin, INPUT);
  pinMode(EndstopSwitchX_Pin, INPUT);

  digitalWrite(SwitchZ_Pin, HIGH);
  digitalWrite(EndstopSwitchZ_Pin, HIGH);
  digitalWrite(EndstopSwitchX_Pin, HIGH);
  digitalWrite(SwitchFoot_Pin, HIGH);

  digitalWrite(Light_Pin, HIGH);
  digitalWrite(DownholderUp_Pin, HIGH);
  digitalWrite(DownholderR_Pin, HIGH);
  digitalWrite(DownholderL_Pin, HIGH);
  digitalWrite(Precharge_Pin, HIGH);

  DisplayInit();
}

void loop() {
  Precharge();
  Display();
  if (digitalRead(SwitchFoot_Pin)) {
    switch (GlobalMode) {
      case 0:
        break;
      case 1:
        JogZ(-1);
        break;
      case 2:
        HomeZ();
        break;
      case 3:
        JogZ(1);
        break;
      case 4:
        JogX(-1);
        break;
      case 5:
        HomeX();
        break;
      case 6:
        JogX(1);
        break;
      case 7:
        JogY(-1);
        break;
      case 8:
        HomeY();
        break;
      case 9:
        JogY(1);
        break;
      case 10:
        GoPosZ(Z_Goalpos, 0);
        drawMode3Page();
        break;
      case 11:
        GoPosX(X_Goalpos, 0);
        break;
      case 12:
        GoPosY(Y_Goalpos, 0);
        break;
      case 13:
        bend();
        break;
      case 14:
        Cut();
        break;
      default:
        break;
    }
  }
}


void HomeZ() {
  digitalWrite(DirH_Pin, LOW);
  digitalWrite(DirL_Pin, HIGH);
  while (digitalRead(SwitchZ_Pin)) {
    Precharge();
    if (digitalRead(SwitchFoot_Pin) && precharged) {
      digitalWrite(StepL_Pin, LOW);
      digitalWrite(StepH_Pin, HIGH);
      delayMicroseconds(50);  // short pulse width only
      digitalWrite(StepL_Pin, HIGH);
      digitalWrite(StepH_Pin, LOW);
      delayMicroseconds(350);  // short pulse width only
    }
  }
  GlobalPos = 0;
  Z_homed = true;
  drawMode3Page();
  GoPosZ(CutHightUp, 1);
  drawMode3Page();
  delay(3000);
}

void HomeX() {
  X_homed = true;
  drawMode3Page();
}

void HomeY() {
  Y_homed = true;
  drawMode3Page();
}


float BendPosition() {
  float alphaDeg = BendAngle + CorrectionAngle;
  float alphaRad = alphaDeg * M_PI / 180.0f;
  const float k = 1.0f / tanf(M_PI / 8.0f);
  float p_ideal = (MatrixWidth / 2.0f) * k * tanf(alphaRad / 4.0f);
  float p_radius = MatrixRadius * (1.0f - cosf(alphaRad / 2.0f));
  float penetration = p_ideal + p_radius;
  Serial.print("Penetration: ");
  Serial.println(penetration);
  float bend = (float)UpperToolHeight - MatrixHeight - MaterialThickness + penetration;
  return bend;
}


void bend() {
  digitalWrite(Light_Pin, LOW);
  float MaterialTouch = (float)UpperToolHeight - float(MatrixHeight) - float(MaterialThickness);

  GoPosZ(MaterialTouch, 1);
  delay(1000);
  float BendDepth = BendPosition();
  GoPosZ(BendDepth, 1);
  delay(500);
  float UpperPos = MaterialTouch - FreeSpace;
  GoPosZ(UpperPos, 1);
  delay(3000);
}

void Cut() {
  digitalWrite(Light_Pin, HIGH);
  downholderDown();
  GoPosZ(CutHightDown, 1);
  delay(1000);
  GoPosZ(CutHightUp, 1);
  downholderUp();
  delay(6000);
}

unsigned long calcSteps(float mm) {
  if (mm < 1) {
    mm = 1;
  }
  if (mm > MaxHight) {
    mm = MaxHight;
  }
  unsigned long steps = mm * float(2929) / float(5);
  return (steps);
}

void downholderDown() {
  //2A = 150 analogRead
  bool moovingR = true;
  bool moovingL = true;
  bool startL = true;
  bool startR = true;
  digitalWrite(DownholderUp_Pin, LOW);
  unsigned int Timer = 1000;
  while ((moovingR || moovingL) && Timer > 0) {
    //Serial.println(analogRead(ShuntR_Pin));
    //Serial.println(analogRead(ShuntL_Pin));
    if (digitalRead(SwitchFoot_Pin)) {
      Timer--;
      if (moovingR) {
        digitalWrite(DownholderR_Pin, LOW);
        if (startR) {
          delay(100);
          startR = false;
        }
        if (analogRead(ShuntR_Pin) > 250) {
          moovingR = false;
          digitalWrite(DownholderR_Pin, HIGH);
        }
      }
      if (moovingL) {
        digitalWrite(DownholderL_Pin, LOW);
        if (startL) {
          delay(100);
          startL = false;
        }
        if (analogRead(ShuntL_Pin) > 250) {
          moovingL = false;
          digitalWrite(DownholderL_Pin, HIGH);
        }
      }
    } else {
      digitalWrite(DownholderR_Pin, HIGH);
      digitalWrite(DownholderL_Pin, HIGH);
      startL = true;
      startR = true;
    }
    delay(5);
  }
}

void downholderUp() {
  digitalWrite(DownholderUp_Pin, HIGH);
  digitalWrite(DownholderR_Pin, HIGH);
  digitalWrite(DownholderL_Pin, HIGH);
}

void JogZ(int direction) {
  if (direction == 1) {
    digitalWrite(DirH_Pin, HIGH);
    digitalWrite(DirL_Pin, LOW);
  } else {
    digitalWrite(DirH_Pin, LOW);
    digitalWrite(DirL_Pin, HIGH);
  }

  while (digitalRead(SwitchFoot_Pin) && precharged) {
    Precharge();
    digitalWrite(StepL_Pin, LOW);
    digitalWrite(StepH_Pin, HIGH);
    delayMicroseconds(50);  // short pulse width only
    digitalWrite(StepL_Pin, HIGH);
    digitalWrite(StepH_Pin, LOW);
    delayMicroseconds(550);  // short pulse width only
    if (direction==1) GlobalPos++;
    else GlobalPos--;
  }
  drawMode3Page();
}

void JogX(int direction) {
  drawMode3Page();
}
void JogY(int direction) {
  drawMode3Page();
}

void GoPosZ(float GoalPos, bool homingMode) {
  unsigned long GoalPosSteps = calcSteps(GoalPos);
  bool direction = (GoalPosSteps > GlobalPos);

  if (direction) {
    digitalWrite(DirH_Pin, HIGH);
    digitalWrite(DirL_Pin, LOW);
  } else {
    digitalWrite(DirH_Pin, LOW);
    digitalWrite(DirL_Pin, HIGH);
  }
  delay(2);

  bool lastPedal = false;
  bool footPedal = false;

  unsigned long now = 0;
  unsigned long lastStepTime = micros();

  uint16_t stepTime = 600;  
  const uint16_t minStepTime = 550;
  uint8_t rampCount = 0;

  while ((abs(GoalPosSteps - GlobalPos) > 0) && (digitalRead(SwitchZ_Pin) || homingMode)) {
    Precharge();
    footPedal = digitalRead(SwitchFoot_Pin);
    if (footPedal && precharged) {
      if (!lastPedal) {
        delay(5);
        lastPedal = true;
        stepTime = 600;
        rampCount = 0;
      }

      now = micros();
      if (now - lastStepTime >= stepTime) {
        lastStepTime = now;
        digitalWrite(StepH_Pin, HIGH);
        digitalWrite(StepL_Pin, LOW);
        delayMicroseconds(20);
        digitalWrite(StepH_Pin, LOW);
        digitalWrite(StepL_Pin, HIGH);

        if (direction) GlobalPos++;
        else GlobalPos--;

        rampCount++;
        if (rampCount == 2) {  // every 16 steps
          rampCount = 0;
          if (stepTime > minStepTime) stepTime--;  // reduce step period
        }
      }

    } else {
      lastPedal = false;  // pedal released → next press restarts ramp
      if(StopAction()){
        return;
      }
    }
  }

  float Posmm = (GlobalPos * 5) / float(2929);
}

void GoPosX(float GoalPos, bool homingMode) {
  return;
}
void GoPosY(float GoalPos, bool homingMode) {
  return;
}


void Precharge() {
  if (digitalRead(Estop_Pin)) {
    if (millis() - PrechargeTimer > PrechargeTime) {
      digitalWrite(Precharge_Pin, LOW);
      precharged = true;
      digitalWrite(FuEnble_Pin, HIGH);
    }
  } else {
    digitalWrite(Precharge_Pin, HIGH);
    precharged = false;
    digitalWrite(FuEnble_Pin, LOW);
    PrechargeTimer = millis();
  }
}
