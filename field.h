#ifndef FIELD_H
#define FIELD_H

#include <stdint.h>

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


#endif

