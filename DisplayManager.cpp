#include "DisplayManager.h"

// Internal state variables (not visible to main program)
static uint8_t selectedSetting = 0;
static uint8_t homingSelection = 0;
static unsigned long lastBlink = 0;
static bool blinkState = false;
String editBuffer;

// Make your LCD accessible to the display module
const char KEY[] = "SPK1 0 0 16 15 1 1023 932 855 789 677 636 599 566 506 483 461 441 404 325 272 234";
SoftPathElectronics keypad;
LiquidCrystal_I2C lcd(0x27, 20, 4);
bool editing = false;


HomingItem homingItems[] = {
  // Z axis
  { 6, 3, "-", 1 },  // Z-
  { 8, 3, "Z", 2 },  // Z home
  { 9, 3, "+", 3 },  // Z+

  // X axis
  { 11, 3, "-", 4 },  // X-
  { 13, 3, "X", 5 },  // X home
  { 14, 3, "+", 6 },  // X+

  // Y axis
  { 16, 3, "-", 7 },  // Y-
  { 18, 3, "Y", 8 },  // Y home
  { 19, 3, "+", 9 }   // Y+
};
uint8_t homingItemCount = sizeof(homingItems) / sizeof(HomingItem);

// Mode 1 fields (positions match your layout)
Field mode1Fields[] = {
  { 3, 0, 4, 0, 6, 2, 0.0, 170.0, &BendAngle },           // Ang:
  { 15, 0, 16, 0, 4, 1, -90.0, 90.0, &CorrectionAngle },  // Cor:
  { 1, 1, 2, 1, 5, 1, 0.0, 700.0, &BendLength },          // L:
  { 9, 1, 10, 1, 4, 2, 0.0, 10.0, &MaterialThickness },   // d:
  { 18, 1, 19, 1, 1, 0, 0.0, 1.0, &Retract },             // Ret:
  { 1, 2, 2, 2, 4, 1, 0.0, 120.0, &MatrixWidth },         // W:
  { 9, 2, 10, 2, 4, 1, 0.0, 120.0, &MatrixHeight },       // H:
  { 16, 2, 17, 2, 3, 1, 0.0, 30.0, &MatrixRadius },       // R:
  { 3, 3, 4, 3, 6, 2, 0.0, 250.0, &StampHeight },         // S_H:
  { 16, 3, 17, 3, 3, 0, 0.0, 100.0, &FreeSpace }          // Free:
};
const uint8_t mode1Count = sizeof(mode1Fields) / sizeof(Field);

// Mode 2 fields
Field mode2Fields[] = {
  // "Cut length: " at (0,0) -> colon at col 10, value at col 12
  { 10, 0, 12, 0, 5, 1, 0.0, 700.0, &CutLength }
};
const uint8_t mode2Count = sizeof(mode2Fields) / sizeof(Field);

// Mode 3 fields (make goal positions editable)
Field mode3Fields[] = {
  // "G:" at (11,0) -> colon at 12, value at 13
  { 12, 0, 13, 0, 6, 2, 2, MaxHight, &Z_Goalpos },
  { 12, 1, 13, 1, 6, 2, 2, MaxHight, &X_Goalpos },
  { 12, 2, 13, 2, 6, 2, 2, MaxHight, &Y_Goalpos }
};
const uint8_t mode3Count = sizeof(mode3Fields) / sizeof(Field);

// helpers to get current field array
Field* currentFields() {
  if (Mode == 1) return mode1Fields;
  if (Mode == 2) return mode2Fields;
  return mode3Fields;
}
uint8_t currentFieldCount() {
  if (Mode == 1) return mode1Count;
  if (Mode == 2) return mode2Count;
  return mode3Count;
}


float Zpos2mm() {
  float Posmm = (GlobalPos * 5) / float(2929);
  return Posmm;
}

void drawHomingLine() {
  lcd.setCursor(0, 3);
  lcd.print("Home: ");

  // Z axis
  lcd.setCursor(7, 3);
  lcd.print("-");
  lcd.setCursor(8, 3);
  lcd.print(Z_homed ? "H" : " ");
  lcd.setCursor(9, 3);
  lcd.print("Z");
  lcd.setCursor(10, 3);
  lcd.print("+");

  // X axis
  lcd.setCursor(15, 3);
  lcd.print("-");
  lcd.setCursor(16, 3);
  lcd.print(X_homed ? "H" : " ");
  lcd.setCursor(17, 3);
  lcd.print("X");
  lcd.setCursor(18, 3);
  lcd.print("+");

  // Y axis
  lcd.setCursor(23, 3);
  lcd.print("-");
  lcd.setCursor(24, 3);
  lcd.print(Y_homed ? "H" : " ");
  lcd.setCursor(25, 3);
  lcd.print("Y");
  lcd.setCursor(26, 3);
  lcd.print("+");
}
void updateHomingBlink() {
  if (Mode != 3) {
    return;
  }

  uint8_t fc = currentFieldCount();
  if (selectedSetting != fc) {
    return;
  }

  unsigned long now = millis();
  if (now - lastBlink >= 400) {
    lastBlink = now;
    blinkState = !blinkState;
    HomingItem& h = homingItems[homingSelection];
    lcd.setCursor(h.col, h.row);
    if (blinkState)
      lcd.print(" ");  // visible debug
    else
      lcd.print(h.text);
  }
}



void nextHomingSelection() {
  // restore old item
  lcd.setCursor(homingItems[homingSelection].col, homingItems[homingSelection].row);
  lcd.print(homingItems[homingSelection].text);

  homingSelection++;
  if (homingSelection >= homingItemCount)
    homingSelection = 0;

  HomingMode = homingItems[homingSelection].modeValue;
}

void printFloatField(const Field& f) {
  lcd.setCursor(f.valCol, f.valRow);

  // Format number into a fixed-width buffer
  char buf[16];
  dtostrf(*(f.value), f.width, f.decimals, buf);

  // Print exactly width characters
  for (uint8_t i = 0; i < f.width; i++) {
    lcd.print(buf[i]);
  }
}



void drawMode1Page() {
  lcd.setCursor(0, 0);
  lcd.print("Ang:");
  printFloatField(mode1Fields[0]);

  lcd.setCursor(12, 0);
  lcd.print("Cor:");
  printFloatField(mode1Fields[1]);

  lcd.setCursor(0, 1);
  lcd.print("L:");
  printFloatField(mode1Fields[2]);

  lcd.setCursor(8, 1);
  lcd.print("d:");
  printFloatField(mode1Fields[3]);

  lcd.setCursor(15, 1);
  lcd.print("Ret:");
  printFloatField(mode1Fields[4]);

  lcd.setCursor(0, 2);
  lcd.print("W:");
  printFloatField(mode1Fields[5]);

  lcd.setCursor(8, 2);
  lcd.print("H:");
  printFloatField(mode1Fields[6]);

  lcd.setCursor(15, 2);
  lcd.print("R:");
  printFloatField(mode1Fields[7]);

  lcd.setCursor(0, 3);
  lcd.print("S_H:");
  printFloatField(mode1Fields[8]);

  lcd.setCursor(12, 3);
  lcd.print("Free:");
  printFloatField(mode1Fields[9]);
}

void drawMode2Page() {
  lcd.setCursor(0, 0);
  lcd.print("Cut length: ");
  printFloatField(mode2Fields[0]);
}

bool StopAction() {
  uint8_t k = keypad.read();
  if (k == KEY_STOP) {
    return 1;
  } else {
    return 0;
  }
}

void drawMode3Page() {
  lcd.setCursor(0, 0);
  lcd.print("Z P:");
  float Zmm = Zpos2mm();
  lcd.print(String(Zmm).substring(0, 6));

  lcd.setCursor(11, 0);
  lcd.print("G:");
  printFloatField(mode3Fields[0]);

  lcd.setCursor(0, 1);
  lcd.print("X P:");
  float Xmm = Zpos2mm();
  lcd.print(String(Xmm).substring(0, 6));

  lcd.setCursor(11, 1);
  lcd.print("G:");
  printFloatField(mode3Fields[1]);

  lcd.setCursor(0, 2);
  lcd.print("Y P:");
  float Ymm = Zpos2mm();
  lcd.print(String(Ymm).substring(0, 6));

  lcd.setCursor(11, 2);
  lcd.print("G:");
  printFloatField(mode3Fields[2]);

  lcd.setCursor(0, 3);
  lcd.print("Home: -");
  lcd.print(Z_homed ? "H" : " ");
  lcd.print("Z+ -");
  lcd.print(X_homed ? "H" : " ");
  lcd.print("X+ -");
  lcd.print(Y_homed ? "H" : " ");
  lcd.print("Y+");
}

void drawCurrentPage() {
  lcd.clear();
  if (Mode == 1) {
    digitalWrite(Light_Pin, LOW);
    drawMode1Page();
  } else if (Mode == 2) {
    digitalWrite(Light_Pin, HIGH);
    drawMode2Page();
  } else if (Mode == 3) {
    digitalWrite(Light_Pin, LOW);
    drawMode3Page();
  }
}

// ----------------- blink handling -----------------
void setColonChar(uint8_t fieldIndex, char c) {
  Field* f = currentFields();
  if (fieldIndex >= currentFieldCount()) return;
  lcd.setCursor(f[fieldIndex].colonCol, f[fieldIndex].colonRow);
  lcd.print(c);
}

void updateBlink() {
  // Do NOT blink colon when in homing selection
  if (selectedSetting == currentFieldCount())
    return;

  if (editing) {
    setColonChar(selectedSetting, ':');
    return;
  }

  unsigned long now = millis();
  if (now - lastBlink >= 400) {
    lastBlink = now;
    blinkState = !blinkState;
    setColonChar(selectedSetting, blinkState ? ' ' : ':');
  }
}


// ----------------- editing -----------------
void startEditingIfNeeded() {
  if (!editing) {
    editing = true;
    editBuffer = "";
    // show colon solid
    setColonChar(selectedSetting, ':');
  }
}

void updateFieldFromBuffer() {
  Field* f = currentFields();
  Field& fld = f[selectedSetting];

  if (editBuffer.length() == 0) {
    editing = false;
    printFloatField(fld);
    return;
  }

  float v = editBuffer.toFloat();

  if (v < fld.minVal) v = fld.minVal;
  if (v > fld.maxVal) v = fld.maxVal;

  *(fld.value) = v;
  editing = false;
  printFloatField(fld);
}

void handleDigit(uint8_t digit) {
  startEditingIfNeeded();
  if (editBuffer.length() >= 6) return;  // simple length limit
  editBuffer += char('0' + digit);

  // live preview
  Field* f = currentFields();
  Field& fld = f[selectedSetting];
  float v = editBuffer.toFloat();
  if (v < fld.minVal) v = fld.minVal;
  if (v > fld.maxVal) v = fld.maxVal;
  *(fld.value) = v;
  printFloatField(fld);
}

void handleDot() {
  startEditingIfNeeded();
  if (editBuffer.indexOf('.') != -1) return;
  if (editBuffer.length() == 0) editBuffer = "0";
  editBuffer += '.';
}

void nextSetting() {
  uint8_t fieldCount = currentFieldCount();
  // Restore previous homing character before moving on
  if (selectedSetting == currentFieldCount()) {
    HomingItem& prev = homingItems[homingSelection];
    lcd.setCursor(prev.col, prev.row);
    lcd.print(prev.text);
  }
  // ----------------------------------------------------
  // CASE 1: We are already in homing selection
  // ----------------------------------------------------
  // CASE: already in homing selection
  if (selectedSetting == fieldCount) {

    // --- Restore the current blinking character before moving on ---
    {
      HomingItem& cur = homingItems[homingSelection];
      lcd.setCursor(cur.col, cur.row);
      lcd.print(cur.text);
    }

    // --- Move to next homing item ---
    homingSelection++;

    // --- If past last item → return to Goalposition Z ---
    if (homingSelection >= homingItemCount) {
      homingSelection = 0;  // reset homing index
      selectedSetting = 0;  // back to first numeric field (Z goal)
      return;
    }

    // --- Otherwise continue blinking next homing item ---
    HomingMode = homingItems[homingSelection].modeValue;
    return;
  }


  // ----------------------------------------------------
  // CASE 2: We are on the last numeric field → ENTER HOMING
  // ----------------------------------------------------
  if (Mode == 3 && selectedSetting == fieldCount - 1) {
    // Restore colon on last numeric field
    setColonChar(selectedSetting, ':');

    // Enter homing selection
    selectedSetting = fieldCount;
    homingSelection = 0;
    HomingMode = homingItems[0].modeValue;
    return;
  }

  // ----------------------------------------------------
  // CASE 3: Normal numeric field navigation
  // ----------------------------------------------------
  if (selectedSetting < fieldCount) {
    // Restore colon on current field
    setColonChar(selectedSetting, ':');

    // Move to next numeric field
    selectedSetting++;

    // Wrap around
    if (selectedSetting >= fieldCount)
      selectedSetting = 0;

    // Set colon on new field
    setColonChar(selectedSetting, ':');
  }
}


void lastSetting() {
  uint8_t fieldCount = currentFieldCount();
  // Restore previous homing character before moving on
  if (selectedSetting == currentFieldCount()) {
    HomingItem& prev = homingItems[homingSelection];
    lcd.setCursor(prev.col, prev.row);
    lcd.print(prev.text);
  }

  // If we are in Mode 3 and currently in homing selection
  if (Mode == 3 && selectedSetting == fieldCount) {
    // Move backwards through homing items
    // Restore current item
    lcd.setCursor(homingItems[homingSelection].col, homingItems[homingSelection].row);
    lcd.print(homingItems[homingSelection].text);

    if (homingSelection == 0)
      homingSelection = homingItemCount - 1;
    else
      homingSelection--;

    HomingMode = homingItems[homingSelection].modeValue;
    return;
  }

  // If we are in Mode 3 and numeric fields are at index 0,
  // jumping backwards should enter homing selection
  if (Mode == 3 && selectedSetting == 0) {
    // restore colon on numeric field 0
    setColonChar(0, ':');

    selectedSetting = fieldCount;  // enter homing selection
    homingSelection = homingItemCount - 1;
    HomingMode = homingItems[homingSelection].modeValue;
    return;
  }

  // Normal numeric-field navigation
  setColonChar(selectedSetting, ':');

  if (selectedSetting == 0)
    selectedSetting = fieldCount - 1;
  else
    selectedSetting--;

  setColonChar(selectedSetting, ':');
}


void ReadKeyboard() {
  uint8_t k = keypad.read();
  if (!k) return;
  switch (k) {
    case KEY_MODE1:
      handleModeKey(1);
      break;
    case KEY_MODE2:
      handleModeKey(2);
      break;
    case KEY_MODE3:
      if (Mode == 3) {
        if (editing) updateFieldFromBuffer();
        nextSetting();
      } else {
        Mode = 3;
        editing = false;
        drawCurrentPage();
        selectedSetting = 0;
        homingSelection = 0;
        HomingMode = 0;
        lastBlink = millis();
        blinkState = false;
      }
      break;

    case KEY_BACK:
      if (editing) updateFieldFromBuffer();
      else lastSetting();
      break;
    case KEY_DOT:
      handleDot();
      break;
    case KEY_0:
      handleDigit(0);
      break;
    case KEY_1:
      handleDigit(1);
      break;
    case KEY_2:
      handleDigit(2);
      break;
    case KEY_3:
      handleDigit(3);
      break;
    case KEY_4:
      handleDigit(4);
      break;
    case KEY_5:
      handleDigit(5);
      break;
    case KEY_6:
      handleDigit(6);
      break;
    case KEY_7:
      handleDigit(7);
      break;
    case KEY_8:
      handleDigit(8);
      break;
    case KEY_9:
      handleDigit(9);
      break;
    default:
      break;
  }
}

void handleModeKey(uint8_t targetMode) {
  // Mode 3 is handled separately
  if (targetMode == 3) return;

  if (Mode == targetMode) {
    if (editing) updateFieldFromBuffer();
    nextSetting();
  } else {
    Mode = targetMode;
    selectedSetting = 0;
    editing = false;
    drawCurrentPage();
  }
}
void DisplayInit() {
  Serial.begin(9600);
  lcd.init();  // initialize the lcd
  lcd.backlight();
  lcd.setCursor(5, 0);
  lcd.print("Press brake");
  lcd.setCursor(6, 1);
  lcd.print("& shears");
  lcd.setCursor(4, 2);
  lcd.print("Version 0.0.1");
  lcd.setCursor(0, 3);
  lcd.print("By Yannick Wunderle");
  delay(1000);
  lcd.clear();
  keypad.loadKey(KEY);
  keypad.setDebug(false);  // Debug-Ausgaben
  keypad.begin();
  drawCurrentPage();
  lastBlink = millis();
}

void CalcGlobalMode() {
  if (selectedSetting < 3) {
    GlobalMode = 10 + selectedSetting;  // 10, 11, 12
  } else {
    GlobalMode = HomingMode;  // 1–9
  }
  if (Mode == 1) GlobalMode = 13;
  if (Mode == 2) GlobalMode = 14;
}

void Display() {
  ReadKeyboard();
  updateBlink();
  updateHomingBlink();
  CalcGlobalMode();
}
