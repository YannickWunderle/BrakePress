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

#define Keyboard_Pin A8  //Also has to be hardcoded in lib
#define Estop_Pin A9
#define SwitchFoot_Pin 5
#define Precharge_Pin 45
#define SwitchZ_Pin 4
#define FuEnble_Pin A13
#define DirH_Pin A4
#define DirL_Pin A5
#define StepH_Pin A6
#define StepL_Pin A7

#define EndstopVcc_Pin 43
#define EndstopL_Pin 7   //13 original 13, remap on mega with wire because of defect pins
#define EndstopH_Pin 10  // 10 original 10, remap on mega with wire because of defect pins
#define EndstopEnX_Pin 12
#define EndstopEnY_Pin 11
#define EndstopTachY_Pin 2
#define EndstopTachX_Pin 3
#define EndstopSwitchY_Pin 8
#define EndstopSwitchX_Pin 9

#define PrechargeTime 2000
#define MaxHight 145
#define UpperToolHeight 167.72  //165.7

#define CutHightUp 60
#define CutHightDown 2
#define MinCutLength 35

#define Zvelmin 3000
#define Zvelmax 200
#define Xvelmin 100
#define Xvelmax 255
#define Yvelmin 220
#define Yvelmax 255

#define XoffsetBending 17
#define YoffsetBending 35
#define XoffsetCutting 17
#define YoffsetCutting 3
#define MaxX 545
#define MaxY 70
#define MaxStepTime 1200
#define MinStepTime 500

bool debug = true;

unsigned long PrechargeTimer = 0;
long GlobalPos = 0;
long GlobalPosX = 800;
long GlobalPosY = 336;

bool precharged = false;

float BendAngle = 90;
float CorrectionAngle = 0;
float BendLength = 100;
float MaterialThickness = 1.25;
float Retract = 0;
float MatrixWidth = 12;
float MatrixHeight = 76;
float MatrixRadius = 0.1;
float StampHeight = 110;
float FreeSpace = 20;
float UseEndstop = true;
float CutLength = 120;

float Z_Goalpos = 60;
float X_Goalpos = 100;
float Y_Goalpos = 42;

bool Z_homed = false;
bool X_homed = false;
bool Y_homed = false;

uint8_t Mode = 3;
uint8_t lastMode = 0;
uint8_t HomingMode = 0;
uint8_t GlobalMode = 0;

float Zvel = 100;
float Xvel = 50;
float Yvel = 50;


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
  pinMode(EndstopEnY_Pin, OUTPUT);
  pinMode(EndstopVcc_Pin, OUTPUT);

  pinMode(ShuntR_Pin, INPUT);
  pinMode(ShuntL_Pin, INPUT);
  pinMode(Estop_Pin, INPUT);
  pinMode(SwitchFoot_Pin, INPUT);
  pinMode(SwitchZ_Pin, INPUT);
  pinMode(EndstopTachY_Pin, INPUT);
  pinMode(EndstopTachX_Pin, INPUT);
  pinMode(EndstopSwitchY_Pin, INPUT);
  pinMode(EndstopSwitchX_Pin, INPUT);

  digitalWrite(SwitchZ_Pin, HIGH);
  digitalWrite(EndstopSwitchY_Pin, HIGH);
  digitalWrite(EndstopSwitchX_Pin, HIGH);
  digitalWrite(SwitchFoot_Pin, HIGH);

  digitalWrite(Light_Pin, HIGH);
  digitalWrite(DownholderUp_Pin, HIGH);
  digitalWrite(DownholderR_Pin, HIGH);
  digitalWrite(DownholderL_Pin, HIGH);
  digitalWrite(Precharge_Pin, HIGH);
  digitalWrite(EndstopVcc_Pin, LOW);

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
        GoPosX(X_Goalpos);
        drawMode3Page();
        break;
      case 12:
        GoPosY(Y_Goalpos);
        drawMode3Page();

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
      delayMicroseconds(MinStepTime);  // short pulse width only
    } else {
      if (StopAction()) {
        return;
      }
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
  digitalWrite(EndstopVcc_Pin, HIGH);
  delay(300);
  digitalWrite(EndstopH_Pin, HIGH);
  digitalWrite(EndstopL_Pin, LOW);
  while (digitalRead(EndstopSwitchX_Pin)) {
    if (digitalRead(SwitchFoot_Pin)) {
      //digitalWrite(EndstopEnX_Pin, HIGH);
      analogWrite(EndstopEnX_Pin, 180);
    } else {
      digitalWrite(EndstopEnX_Pin, LOW);
      if (StopAction()) {
        return;
      }
    }
  }
  digitalWrite(EndstopEnX_Pin, LOW);
  GlobalPosX = 0;
  SpindownX(0);
  X_homed = true;
  drawMode3Page();
  GoPosX(100);
  drawMode3Page();
  delay(2000);
  digitalWrite(EndstopVcc_Pin, LOW);
}

void SpindownX(int direction) {
  unsigned long spindown = millis();
  bool LaststateX = digitalRead(EndstopTachX_Pin);
  while ((millis() - spindown) < 500) {
    if (digitalRead(EndstopTachX_Pin) != LaststateX) {
      LaststateX = !LaststateX;
      if (direction == 1) {
        GlobalPosX++;
      } else {
        GlobalPosX--;
      }
    }
  }
}

void SpindownY(int direction) {
  unsigned long spindown = millis();
  bool LaststateY = digitalRead(EndstopTachY_Pin);
  while ((millis() - spindown) < 500) {
    if (digitalRead(EndstopTachY_Pin) != LaststateY) {
      LaststateY = !LaststateY;
      if (direction == 1) {
        GlobalPosY++;
      } else {
        GlobalPosY--;
      }
    }
  }
}

void GoPosX(float positionMM) {
  delay(300);
  positionMM = constrain(positionMM, 0, MaxX);
  long GoalPos = calcXsteps(positionMM);
  if (GoalPos == GlobalPosX) {
    return;
  }
  digitalWrite(EndstopVcc_Pin, HIGH);

  while (GoalPos != GlobalPosX) {
    int direction = 0;
    if (GoalPos > GlobalPosX) {
      direction = 1;
    } else {
      direction = 0;
    }
    if (direction == 1) {
      digitalWrite(EndstopH_Pin, LOW);
      digitalWrite(EndstopL_Pin, HIGH);
    } else {
      digitalWrite(EndstopH_Pin, HIGH);
      digitalWrite(EndstopL_Pin, LOW);
    }
    int velocity = 200;  //map(Xvel, 0, 100, Xvelmin, Xvelmax);
    int slowvelocity = velocity;
    bool LaststateX = digitalRead(EndstopTachX_Pin);
    long distance = abs(GoalPos - GlobalPosX);
    while (distance > 0) {
      distance = abs(GoalPos - GlobalPosX);
      if (digitalRead(SwitchFoot_Pin)) {
        if (distance > 55) {
          analogWrite(EndstopEnX_Pin, velocity);
        } else {
          slowvelocity = map(distance, 0, 55, 60, velocity);
          analogWrite(EndstopEnX_Pin, slowvelocity);
        }
        //Serial.println(GlobalPosX);
        if (digitalRead(EndstopTachX_Pin) != LaststateX) {
          LaststateX = !LaststateX;
          if (direction == 1) {
            GlobalPosX++;
          } else {
            GlobalPosX--;
          }
        }
      } else {
        digitalWrite(EndstopEnX_Pin, LOW);
        SpindownX(direction);
        if (StopAction()) {
          digitalWrite(EndstopVcc_Pin, LOW);
          return;
        }
      }
    }
    digitalWrite(EndstopH_Pin, LOW);
    digitalWrite(EndstopL_Pin, LOW);
    SpindownX(direction);
    digitalWrite(EndstopEnX_Pin, LOW);
  }
  digitalWrite(EndstopVcc_Pin, LOW);
}

long calcXsteps(float mm) {
  long Goalpos = mm * float(8);
  return Goalpos;
}

void HomeY() {
  digitalWrite(EndstopVcc_Pin, HIGH);
  delay(300);
  digitalWrite(EndstopH_Pin, HIGH);
  digitalWrite(EndstopL_Pin, LOW);
  while (digitalRead(EndstopSwitchY_Pin)) {
    if (digitalRead(SwitchFoot_Pin)) {
      //digitalWrite(EndstopEnX_Pin, HIGH);
      analogWrite(EndstopEnY_Pin, 170);
    } else {
      digitalWrite(EndstopEnY_Pin, LOW);
      if (StopAction()) {
        return;
      }
    }
  }
  digitalWrite(EndstopEnY_Pin, LOW);
  GlobalPosY = 0;
  SpindownY(0);
  Y_homed = true;
  drawMode3Page();
  GoPosY(42);
  drawMode3Page();
  delay(2000);
  digitalWrite(EndstopVcc_Pin, LOW);
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
  float MaterialTouch = (float)UpperToolHeight - float(MatrixHeight) - float(MaterialThickness);
  if (UseEndstop) {
    if (calcXsteps(BendLength + XoffsetBending) != GlobalPosX) {
      if (BendLength < 120) {
        GoPosY(MatrixHeight - YoffsetBending + 5);
        GoPosX(BendLength + XoffsetBending);
        GoPosY(MatrixHeight - YoffsetBending);
      } else {
        GoPosX(BendLength + XoffsetBending - 60);
        GoPosY(MatrixHeight - YoffsetBending - 5);
      }
    }
    if (Retract) {
      GoPosZ(MaterialTouch, 1);
      if (BendLength < 120) {
        GoPosY(MatrixHeight - YoffsetBending + 5);
        GoPosX(BendLength + XoffsetBending + Retract * 10);
      } else {
        GoPosX(BendLength + XoffsetBending + Retract * 10);
      }
    }
  }
  float BendDepth = BendPosition();
  GoPosZ(BendDepth, 1);
  delay(500);
  float UpperPos = MaterialTouch - FreeSpace;
  GoPosZ(UpperPos, 1);
  delay(3000);
}

void Cut() {
  CutLength = constrain(CutLength, MinCutLength, 545);
  if (UseEndstop) {
    if (CutLength < 120) {
      GoPosX(CutLength + XoffsetCutting);
      GoPosY(YoffsetCutting);
    } else {
      GoPosX(CutLength + XoffsetCutting - 55);
      GoPosY(YoffsetCutting + 5);
    }
    delay(2000);
  }
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
      if (StopAction()) {
        return;
      }
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
  int stepTime = map(Zvel, 0, 100, MaxStepTime, MinStepTime);
  unsigned long now = 0;
  unsigned long lastStepTime = micros();


  while (digitalRead(SwitchFoot_Pin) && precharged) {
    Precharge();
    now = micros();
    if (now - lastStepTime >= stepTime) {
      lastStepTime = now;
      digitalWrite(StepL_Pin, LOW);
      digitalWrite(StepH_Pin, HIGH);
      delayMicroseconds(20);  // short pulse width only
      digitalWrite(StepL_Pin, HIGH);
      digitalWrite(StepH_Pin, LOW);
      if (direction == 1) GlobalPos++;
      else GlobalPos--;
    }
  }
  drawMode3Page();
}

void JogX(int direction) {
  digitalWrite(EndstopVcc_Pin, HIGH);
  delay(300);
  if (direction == 1) {
    digitalWrite(EndstopH_Pin, LOW);
    digitalWrite(EndstopL_Pin, HIGH);
  } else {
    digitalWrite(EndstopH_Pin, HIGH);
    digitalWrite(EndstopL_Pin, LOW);
  }
  int velocity = map(Xvel, 0, 100, Xvelmin, Xvelmax);
  bool LaststateX = digitalRead(EndstopTachX_Pin);
  while (digitalRead(SwitchFoot_Pin)) {
    //digitalWrite(EndstopEnX_Pin, HIGH);
    analogWrite(EndstopEnX_Pin, velocity);
    //Serial.println(GlobalPosX);
    if (digitalRead(EndstopTachX_Pin) != LaststateX) {
      LaststateX = !LaststateX;
      if (direction == 1) {
        GlobalPosX++;
      } else {
        GlobalPosX--;
      }
    }
  }
  digitalWrite(EndstopH_Pin, LOW);
  digitalWrite(EndstopL_Pin, LOW);
  SpindownX(direction);
  digitalWrite(EndstopEnX_Pin, LOW);
  drawMode3Page();
  digitalWrite(EndstopVcc_Pin, LOW);
  ;
}

void JogY(int direction) {
  digitalWrite(EndstopVcc_Pin, HIGH);
  delay(300);
  if (direction == 1) {
    digitalWrite(EndstopH_Pin, LOW);
    digitalWrite(EndstopL_Pin, HIGH);
  } else {
    digitalWrite(EndstopH_Pin, HIGH);
    digitalWrite(EndstopL_Pin, LOW);
  }
  int velocity = map(Yvel, 0, 100, Yvelmin, Yvelmax);
  bool LaststateY = digitalRead(EndstopTachY_Pin);
  while (digitalRead(SwitchFoot_Pin)) {
    analogWrite(EndstopEnY_Pin, velocity);
    if (digitalRead(EndstopTachY_Pin) != LaststateY) {
      LaststateY = !LaststateY;
      if (direction == 1) {
        GlobalPosY++;
      } else {
        GlobalPosY--;
      }
    }
  }
  digitalWrite(EndstopH_Pin, LOW);
  digitalWrite(EndstopL_Pin, LOW);
  SpindownY(direction);
  digitalWrite(EndstopEnY_Pin, LOW);
  drawMode3Page();
  digitalWrite(EndstopVcc_Pin, LOW);
}

void GoPosZ(float GoalPos, bool homingMode) {
  GoalPos = constrain(GoalPos, 2, MaxHight);
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

  uint16_t stepTime = map(Zvel, 0, 100, MaxStepTime, MinStepTime);
  //const uint16_t minStepTime = 550;
  uint8_t rampCount = 0;

  while ((abs(GoalPosSteps - GlobalPos) > 0) && (digitalRead(SwitchZ_Pin) || homingMode)) {
    Precharge();
    footPedal = digitalRead(SwitchFoot_Pin);
    if (footPedal && precharged) {
      if (!lastPedal) {
        delay(5);
        lastPedal = true;
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

        /*rampCount++;
        if (rampCount == 2) {
          rampCount = 0;
          if (stepTime > MinStepTime) stepTime--;  // reduce step period
        }*/
      }

    } else {
      lastPedal = false;  // pedal released → next press restarts ramp
      if (StopAction()) {
        return;
      }
    }
  }

  float Posmm = (GlobalPos * 5) / float(2929);
}

void GoPosY(float positionMM) {
  delay(300);
  positionMM = constrain(positionMM, 0, MaxY);
  long GoalPos = calcXsteps(positionMM);
  if (GoalPos == GlobalPosY) {
    return;
  }
  digitalWrite(EndstopVcc_Pin, HIGH);

  while (GoalPos != GlobalPosY) {
    int direction = 0;
    if (GoalPos > GlobalPosY) {
      direction = 1;
    } else {
      direction = 0;
    }
    if (direction == 1) {
      digitalWrite(EndstopH_Pin, LOW);
      digitalWrite(EndstopL_Pin, HIGH);
    } else {
      digitalWrite(EndstopH_Pin, HIGH);
      digitalWrite(EndstopL_Pin, LOW);
    }
    int velocity = 255;  //map(Xvel, 0, 100, Xvelmin, Xvelmax);
    int slowvelocity = velocity;
    bool LaststateY = digitalRead(EndstopTachY_Pin);
    long distance = abs(GoalPos - GlobalPosY);
    while (distance > 0) {
      distance = abs(GoalPos - GlobalPosY);
      if (digitalRead(SwitchFoot_Pin)) {
        if (distance > 50) {
          analogWrite(EndstopEnY_Pin, velocity);
        } else {
          slowvelocity = map(distance, 0, 50, 170, velocity);
          analogWrite(EndstopEnY_Pin, slowvelocity);
        }
        //Serial.println(GlobalPosX);
        if (digitalRead(EndstopTachY_Pin) != LaststateY) {
          LaststateY = !LaststateY;
          if (direction == 1) {
            GlobalPosY++;
          } else {
            GlobalPosY--;
          }
        }
      } else {
        digitalWrite(EndstopEnY_Pin, LOW);
        SpindownX(direction);
        if (StopAction()) {
          digitalWrite(EndstopVcc_Pin, LOW);
          return;
        }
      }
    }
    digitalWrite(EndstopH_Pin, LOW);
    digitalWrite(EndstopL_Pin, LOW);
    SpindownX(direction);
    digitalWrite(EndstopEnY_Pin, LOW);
  }
  digitalWrite(EndstopVcc_Pin, LOW);
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
