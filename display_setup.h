// Purpose: Declares the display abstraction and native drawing interface used by
// the MJPEG player.
#pragma once

#include <Arduino.h>
#include "app_config.h"

#ifndef RGB565_BLACK
#define RGB565_BLACK 0x0000
#endif

#ifndef RGB565_WHITE
#define RGB565_WHITE 0xFFFF
#endif

class DisplayDriver
{
public:
  bool begin();
  int16_t width() const;
  int16_t height() const;

  void fillScreen(uint16_t color);
  void draw16bitRGBBitmap(int32_t x, int32_t y, const uint16_t *bitmap, int32_t w, int32_t h);
  void draw16bitBeRGBBitmap(int32_t x, int32_t y, const uint16_t *bitmap, int32_t w, int32_t h);

  void setTextColor(uint16_t color);
  void setTextSize(uint8_t size);
  void setCursor(int16_t x, int16_t y);
  size_t print(const char *text);
  size_t println(const char *text);

private:
  uint16_t _textColor = RGB565_WHITE;
  uint8_t _textSize = 1;
  int16_t _cursorX = 0;
  int16_t _cursorY = 0;
};

extern DisplayDriver *gfx;

bool initDisplay();
void draw16bitRGBBitmapFastNoClip(int32_t x, int32_t y, const uint16_t *bitmap, int32_t w, int32_t h);
