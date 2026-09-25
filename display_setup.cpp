// Purpose: Initializes the native ST7701 display path and provides drawing helpers
// used by the MJPEG player and status UI.
#include "display_setup.h"

#include <Arduino.h>

static void setBacklight(uint8_t duty_8bit)
{
  // This board's backlight PWM is active-low (vendor demo uses 0xFF - level).
  uint8_t hw_duty = 255 - duty_8bit;

  if (ledcAttach(LCD_PIN_BACKLIGHT, LCD_BACKLIGHT_PWM_FREQ_HZ, 8))
  {
    ledcWrite(LCD_PIN_BACKLIGHT, hw_duty);
  }
  else
  {
    pinMode(LCD_PIN_BACKLIGHT, OUTPUT);
    digitalWrite(LCD_PIN_BACKLIGHT, duty_8bit ? LOW : HIGH);
  }
}

extern "C"
{
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_st7701.h"
}

#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <cstring>

static DisplayDriver nativeDisplay;
DisplayDriver *gfx = &nativeDisplay;

static esp_lcd_panel_handle_t nativePanel = nullptr;
static uint16_t *nativeLineBuffer = nullptr;
static bool nativeDisplayReady = false;
static const uint8_t kNativeFontWidth = 5;
static const uint8_t kNativeFontHeight = 7;
static const uint8_t kNativeFontAdvance = 6;

// Minimal 5x7 uppercase font for centered status text on the native backend.
static const uint8_t kNativeUppercaseFont[][kNativeFontHeight] = {
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // A
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, // B
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, // C
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, // D
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, // E
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, // F
    {0x0E, 0x11, 0x10, 0x10, 0x13, 0x11, 0x0F}, // G
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // H
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F}, // I
    {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E}, // J
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, // K
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, // L
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, // M
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, // N
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // O
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, // P
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, // Q
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, // R
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, // S
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // T
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // U
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, // V
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}, // W
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, // X
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, // Y
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, // Z
};
static const uint8_t kNativeSpaceGlyph[kNativeFontHeight] = {0, 0, 0, 0, 0, 0, 0};

static const uint8_t kSt7701RgbctrlC300 =
    (uint8_t)(((IDF_LCD_VSYNC_ACTIVE_HIGH ? 1 : 0) << 3) |
              ((IDF_LCD_HSYNC_ACTIVE_HIGH ? 1 : 0) << 2) |
              ((IDF_LCD_PCLK_ACTIVE_NEG ? 1 : 0) << 1) |
              (IDF_LCD_DE_ACTIVE_HIGH ? 0 : 1));
static const uint8_t kSt7701RgbctrlC301 = 0x10;
static const uint8_t kSt7701RgbctrlC302 = 0x08;

// Manufacturer-provided init sequence for this specific 320x820 ST7701 board.
static const st7701_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0xE5, 0x02}, 2, 0},
    {0xC1, (uint8_t[]){0x15, 0x0A}, 2, 0},
    {0xC2, (uint8_t[]){0x07, 0x02}, 2, 0},
    {0xC3, (uint8_t[]){kSt7701RgbctrlC300, kSt7701RgbctrlC301, kSt7701RgbctrlC302}, 3, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},
    {0xB0, (uint8_t[]){0x00, 0x08, 0x51, 0x0D, 0xCE, 0x06, 0x00, 0x08, 0x08, 0x24, 0x05, 0xD0, 0x0F, 0x6F, 0x36, 0x1F},
     16, 0},
    {0xB1, (uint8_t[]){0x00, 0x10, 0x4F, 0x0C, 0x11, 0x05, 0x00, 0x07, 0x07, 0x18, 0x02, 0xD3, 0x11, 0x6E, 0x34, 0x1F},
     16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x4D}, 1, 0},
    {0xB1, (uint8_t[]){0x37}, 1, 0},
    {0xB2, (uint8_t[]){0x87}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x4A}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},
    {0xB9, (uint8_t[]){0x00, 0x13}, 2, 0},
    {0xC0, (uint8_t[]){0x09}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 0},
    {0xE0, (uint8_t[]){0x80, 0x00, 0x02}, 3, 100},
    {0xE1, (uint8_t[]){0x0F, 0xA0, 0x00, 0x00, 0x10, 0xA0, 0x00, 0x00, 0x00, 0x60, 0x60}, 11, 0},
    {0xE2, (uint8_t[]){0x30, 0x30, 0x60, 0x60, 0x45, 0xA0, 0x00, 0x00, 0x46, 0xA0, 0x00, 0x00, 0x00}, 13, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x0F, 0x4A, 0xA0, 0xA0, 0x11, 0x4A, 0xA0, 0xA0, 0x13, 0x4A, 0xA0, 0xA0, 0x15, 0x4A, 0xA0, 0xA0},
     16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x10, 0x4A, 0xA0, 0xA0, 0x12, 0x4A, 0xA0, 0xA0, 0x14, 0x4A, 0xA0, 0xA0, 0x16, 0x4A, 0xA0, 0xA0},
     16, 0},
    {0xEB, (uint8_t[]){0x02, 0x00, 0x4E, 0x4E, 0xEE, 0x44, 0x00}, 7, 0},
    {0xED, (uint8_t[]){0xFF, 0xFF, 0x04, 0x56, 0x72, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x27, 0x65, 0x40, 0xFF, 0xFF},
     16, 0},
    {0xEF, (uint8_t[]){0x08, 0x08, 0x08, 0x40, 0x3F, 0x64}, 6, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xE8, (uint8_t[]){0x00, 0x0E}, 2, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xE8, (uint8_t[]){0x00, 0x0C}, 2, 10},
    {0xE8, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0x36, (uint8_t[]){0x00}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x29, (uint8_t[]){0x00}, 0, 20},
};

static bool initNativePanel()
{
  spi_line_config_t lineConfig = {};
  lineConfig.cs_io_type = IO_TYPE_GPIO;
  lineConfig.cs_gpio_num = LCD_PIN_SPI_CS;
  lineConfig.scl_io_type = IO_TYPE_GPIO;
  lineConfig.scl_gpio_num = LCD_PIN_SPI_SCK;
  lineConfig.sda_io_type = IO_TYPE_GPIO;
  lineConfig.sda_gpio_num = LCD_PIN_SPI_SDO;
  lineConfig.io_expander = NULL;

  esp_lcd_panel_io_3wire_spi_config_t ioConfig = {};
  ioConfig.line_config = lineConfig;
  ioConfig.expect_clk_speed = PANEL_IO_3WIRE_SPI_CLK_MAX;
  ioConfig.spi_mode = 0;
  ioConfig.lcd_cmd_bytes = 1;
  ioConfig.lcd_param_bytes = 1;
  ioConfig.flags.use_dc_bit = 1;
  ioConfig.flags.dc_zero_on_data = 0;
  ioConfig.flags.lsb_first = 0;
  ioConfig.flags.cs_high_active = 0;
  ioConfig.flags.del_keep_cs_inactive = 1;
  esp_lcd_panel_io_handle_t ioHandle = NULL;
  if (esp_lcd_new_panel_io_3wire_spi(&ioConfig, &ioHandle) != ESP_OK)
  {
    Serial.println("native display: panel IO create failed");
    return false;
  }

  esp_lcd_rgb_panel_config_t rgbConfig = {};
  rgbConfig.clk_src = LCD_CLK_SRC_DEFAULT;
  rgbConfig.psram_trans_align = 64;
  rgbConfig.bounce_buffer_size_px = 10 * LCD_H_RES;
  rgbConfig.num_fbs = 2;
  rgbConfig.data_width = 16;
  rgbConfig.bits_per_pixel = 16;
  rgbConfig.de_gpio_num = LCD_PIN_DE;
  rgbConfig.pclk_gpio_num = LCD_PIN_PCLK;
  rgbConfig.vsync_gpio_num = LCD_PIN_VSYNC;
  rgbConfig.hsync_gpio_num = LCD_PIN_HSYNC;
  rgbConfig.disp_gpio_num = -1;
  rgbConfig.flags.fb_in_psram = true;

  // Vendor example maps bus bits as B-G-R for this board wiring.
  rgbConfig.data_gpio_nums[0] = LCD_PIN_B0;
  rgbConfig.data_gpio_nums[1] = LCD_PIN_B1;
  rgbConfig.data_gpio_nums[2] = LCD_PIN_B2;
  rgbConfig.data_gpio_nums[3] = LCD_PIN_B3;
  rgbConfig.data_gpio_nums[4] = LCD_PIN_B4;
  rgbConfig.data_gpio_nums[5] = LCD_PIN_G0;
  rgbConfig.data_gpio_nums[6] = LCD_PIN_G1;
  rgbConfig.data_gpio_nums[7] = LCD_PIN_G2;
  rgbConfig.data_gpio_nums[8] = LCD_PIN_G3;
  rgbConfig.data_gpio_nums[9] = LCD_PIN_G4;
  rgbConfig.data_gpio_nums[10] = LCD_PIN_G5;
  rgbConfig.data_gpio_nums[11] = LCD_PIN_R0;
  rgbConfig.data_gpio_nums[12] = LCD_PIN_R1;
  rgbConfig.data_gpio_nums[13] = LCD_PIN_R2;
  rgbConfig.data_gpio_nums[14] = LCD_PIN_R3;
  rgbConfig.data_gpio_nums[15] = LCD_PIN_R4;

  rgbConfig.timings.pclk_hz = IDF_LCD_PCLK_HZ;
  rgbConfig.timings.h_res = LCD_H_RES;
  rgbConfig.timings.v_res = LCD_V_RES;
  rgbConfig.timings.hsync_back_porch = IDF_LCD_HSYNC_BACK_PORCH;
  rgbConfig.timings.hsync_front_porch = IDF_LCD_HSYNC_FRONT_PORCH;
  rgbConfig.timings.hsync_pulse_width = IDF_LCD_HSYNC_PULSE_WIDTH;
  rgbConfig.timings.vsync_back_porch = IDF_LCD_VSYNC_BACK_PORCH;
  rgbConfig.timings.vsync_front_porch = IDF_LCD_VSYNC_FRONT_PORCH;
  rgbConfig.timings.vsync_pulse_width = IDF_LCD_VSYNC_PULSE_WIDTH;
  rgbConfig.timings.flags.hsync_idle_low = IDF_LCD_HSYNC_ACTIVE_HIGH ? 1 : 0;
  rgbConfig.timings.flags.vsync_idle_low = IDF_LCD_VSYNC_ACTIVE_HIGH ? 1 : 0;
  rgbConfig.timings.flags.de_idle_high = IDF_LCD_DE_ACTIVE_HIGH ? 0 : 1;
  rgbConfig.timings.flags.pclk_active_neg = IDF_LCD_PCLK_ACTIVE_NEG ? 1 : 0;

  st7701_vendor_config_t vendorConfig = {};
  vendorConfig.rgb_config = &rgbConfig;
  vendorConfig.init_cmds = lcd_init_cmds;
  vendorConfig.init_cmds_size = sizeof(lcd_init_cmds) / sizeof(st7701_lcd_init_cmd_t);
  vendorConfig.flags.enable_io_multiplex = IDF_LCD_RELEASE_PANEL_IO_AFTER_INIT ? 1 : 0;
  vendorConfig.flags.mirror_by_cmd = IDF_LCD_RELEASE_PANEL_IO_AFTER_INIT ? 0 : 1;

  esp_lcd_panel_dev_config_t panelConfig = {};
  panelConfig.reset_gpio_num = LCD_PIN_RESET;
  panelConfig.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  panelConfig.bits_per_pixel = 16;
  panelConfig.vendor_config = &vendorConfig;

  if (esp_lcd_new_panel_st7701(ioHandle, &panelConfig, &nativePanel) != ESP_OK)
  {
    Serial.println("native display: st7701 create failed");
    esp_lcd_panel_io_del(ioHandle);
    return false;
  }

  if (IDF_LCD_RELEASE_PANEL_IO_AFTER_INIT)
  {
    // In IO-multiplex mode, ST7701 init commands are sent during panel creation,
    // then panel IO is deleted. A later hardware reset would clear that state.
    if (esp_lcd_panel_init(nativePanel) != ESP_OK)
    {
      Serial.println("native display: panel init failed");
      return false;
    }
  }
  else
  {
    if (esp_lcd_panel_reset(nativePanel) != ESP_OK || esp_lcd_panel_init(nativePanel) != ESP_OK)
    {
      Serial.println("native display: panel reset/init failed");
      return false;
    }
  }

  if (LCD_ROTATION != 0)
  {
    Serial.println("native display: LCD_ROTATION is ignored in native backend");
  }

  nativeLineBuffer =
      (uint16_t *)heap_caps_malloc((size_t)LCD_H_RES * sizeof(uint16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (nativeLineBuffer == NULL)
  {
    nativeLineBuffer = (uint16_t *)malloc((size_t)LCD_H_RES * sizeof(uint16_t));
  }
  if (nativeLineBuffer == NULL)
  {
    Serial.println("native display: line buffer alloc failed");
    return false;
  }

  nativeDisplayReady = true;
  return true;
}

bool DisplayDriver::begin()
{
  if (nativeDisplayReady)
  {
    return true;
  }
  return initNativePanel();
}

int16_t DisplayDriver::width() const
{
  return LCD_H_RES;
}

int16_t DisplayDriver::height() const
{
  return LCD_V_RES;
}

static void drawBitmapClipped(int32_t x, int32_t y, const uint16_t *bitmap, int32_t srcW, int32_t srcH, bool sourceBigEndian)
{
  if (!nativeDisplayReady || nativePanel == NULL || bitmap == NULL || srcW <= 0 || srcH <= 0)
  {
    return;
  }

  int32_t dstX = x;
  int32_t dstY = y;
  int32_t drawW = srcW;
  int32_t drawH = srcH;
  int32_t srcOffsetX = 0;
  int32_t srcOffsetY = 0;

  if (dstX < 0)
  {
    srcOffsetX = -dstX;
    drawW += dstX;
    dstX = 0;
  }
  if (dstY < 0)
  {
    srcOffsetY = -dstY;
    drawH += dstY;
    dstY = 0;
  }
  if ((dstX + drawW) > LCD_H_RES)
  {
    drawW = LCD_H_RES - dstX;
  }
  if ((dstY + drawH) > LCD_V_RES)
  {
    drawH = LCD_V_RES - dstY;
  }
  if (drawW <= 0 || drawH <= 0)
  {
    return;
  }

  bool sourceIsContiguous = (srcOffsetX == 0) && (drawW == srcW);
  if (!sourceBigEndian && sourceIsContiguous)
  {
    const uint16_t *srcBlock = bitmap + (srcOffsetY * srcW);
    esp_lcd_panel_draw_bitmap(nativePanel, dstX, dstY, dstX + drawW, dstY + drawH, (void *)srcBlock);
    return;
  }

  for (int32_t row = 0; row < drawH; row++)
  {
    const uint16_t *srcRow = bitmap + ((srcOffsetY + row) * srcW) + srcOffsetX;
    void *rowData = (void *)srcRow;

    if (sourceBigEndian)
    {
      if (nativeLineBuffer == NULL)
      {
        return;
      }
      for (int32_t col = 0; col < drawW; col++)
      {
        uint16_t px = srcRow[col];
        nativeLineBuffer[col] = (uint16_t)((px >> 8) | (px << 8));
      }
      rowData = (void *)nativeLineBuffer;
    }

    esp_lcd_panel_draw_bitmap(nativePanel, dstX, dstY + row, dstX + drawW, dstY + row + 1, rowData);
  }
}

static void fillSolidRectClipped(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color)
{
  if (!nativeDisplayReady || nativePanel == NULL || nativeLineBuffer == NULL || w <= 0 || h <= 0)
  {
    return;
  }

  int32_t dstX = x;
  int32_t dstY = y;
  int32_t drawW = w;
  int32_t drawH = h;

  if (dstX < 0)
  {
    drawW += dstX;
    dstX = 0;
  }
  if (dstY < 0)
  {
    drawH += dstY;
    dstY = 0;
  }
  if ((dstX + drawW) > LCD_H_RES)
  {
    drawW = LCD_H_RES - dstX;
  }
  if ((dstY + drawH) > LCD_V_RES)
  {
    drawH = LCD_V_RES - dstY;
  }
  if (drawW <= 0 || drawH <= 0)
  {
    return;
  }

  for (int32_t col = 0; col < drawW; col++)
  {
    nativeLineBuffer[col] = color;
  }

  for (int32_t row = 0; row < drawH; row++)
  {
    esp_lcd_panel_draw_bitmap(nativePanel, dstX, dstY + row, dstX + drawW, dstY + row + 1, nativeLineBuffer);
  }
}

static const uint8_t *lookupNativeGlyph(char c)
{
  if (c >= 'a' && c <= 'z')
  {
    c = (char)(c - ('a' - 'A'));
  }

  if (c >= 'A' && c <= 'Z')
  {
    return kNativeUppercaseFont[c - 'A'];
  }

  if (c == ' ')
  {
    return kNativeSpaceGlyph;
  }

  return kNativeSpaceGlyph;
}

static void drawNativeTextChar(int32_t x, int32_t y, char c, uint16_t color, uint8_t scale)
{
  const uint8_t *glyph = lookupNativeGlyph(c);
  if (glyph == NULL)
  {
    return;
  }

  if (scale == 0)
  {
    scale = 1;
  }

  for (uint8_t row = 0; row < kNativeFontHeight; row++)
  {
    uint8_t bits = glyph[row];
    for (uint8_t col = 0; col < kNativeFontWidth; col++)
    {
      if ((bits & (1U << (kNativeFontWidth - 1 - col))) == 0)
      {
        continue;
      }

      fillSolidRectClipped(
          x + (int32_t)col * scale,
          y + (int32_t)row * scale,
          scale,
          scale,
          color);
    }
  }
}

void DisplayDriver::fillScreen(uint16_t color)
{
  if (!nativeDisplayReady || nativePanel == NULL || nativeLineBuffer == NULL)
  {
    return;
  }

  for (int32_t x = 0; x < LCD_H_RES; x++)
  {
    nativeLineBuffer[x] = color;
  }

  for (int32_t y = 0; y < LCD_V_RES; y++)
  {
    esp_lcd_panel_draw_bitmap(nativePanel, 0, y, LCD_H_RES, y + 1, nativeLineBuffer);
  }
}

void DisplayDriver::draw16bitRGBBitmap(int32_t x, int32_t y, const uint16_t *bitmap, int32_t w, int32_t h)
{
  drawBitmapClipped(x, y, bitmap, w, h, false);
}

void DisplayDriver::draw16bitBeRGBBitmap(int32_t x, int32_t y, const uint16_t *bitmap, int32_t w, int32_t h)
{
  drawBitmapClipped(x, y, bitmap, w, h, true);
}

void draw16bitRGBBitmapFastNoClip(int32_t x, int32_t y, const uint16_t *bitmap, int32_t w, int32_t h)
{
  if (bitmap == NULL || w <= 0 || h <= 0)
  {
    return;
  }

  drawBitmapClipped(x, y, bitmap, w, h, false);
}

void DisplayDriver::setTextColor(uint16_t color)
{
  _textColor = color;
}

void DisplayDriver::setTextSize(uint8_t size)
{
  _textSize = size;
}

void DisplayDriver::setCursor(int16_t x, int16_t y)
{
  _cursorX = x;
  _cursorY = y;
}

size_t DisplayDriver::print(const char *text)
{
  if (text == NULL)
  {
    return 0;
  }

  size_t written = 0;
  int16_t cursorX = _cursorX;
  int16_t cursorY = _cursorY;
  uint8_t scale = (_textSize == 0) ? 1 : _textSize;
  int16_t lineAdvance = (int16_t)kNativeFontAdvance * scale;
  int16_t rowAdvance = (int16_t)(kNativeFontHeight + 1) * scale;

  while (*text)
  {
    char c = *text++;
    if (c == '\r')
    {
      continue;
    }
    if (c == '\n')
    {
      cursorX = _cursorX;
      cursorY = (int16_t)(cursorY + rowAdvance);
      continue;
    }

    drawNativeTextChar(cursorX, cursorY, c, _textColor, scale);
    cursorX = (int16_t)(cursorX + lineAdvance);
    written++;
  }

  _cursorX = cursorX;
  _cursorY = cursorY;
  return written;
}

size_t DisplayDriver::println(const char *text)
{
  if (text == NULL)
  {
    _cursorX = 0;
    _cursorY = (int16_t)(_cursorY + (int16_t)(kNativeFontHeight + 1) * ((_textSize == 0) ? 1 : _textSize));
    return 1;
  }

  size_t written = print(text);
  _cursorX = 0;
  _cursorY = (int16_t)(_cursorY + (int16_t)(kNativeFontHeight + 1) * ((_textSize == 0) ? 1 : _textSize));
  return written + 1;
}

bool initDisplay()
{
  setBacklight(LCD_BACKLIGHT_DEFAULT_BRIGHTNESS);
  return gfx->begin();
}
