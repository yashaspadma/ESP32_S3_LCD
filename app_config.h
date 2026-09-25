// Purpose: User-facing app settings plus advanced tuning and hardware configuration
// for the ESP32-S3 MJPEG SD player.
#pragma once

// ---------------------------------------------------------------------------
// User App Configuration
// These are the settings most users are expected to change.
// ---------------------------------------------------------------------------

// Folder on the SD card to scan for playable .mjpeg / .mjpg files.
// Example: "/mjpeg"
#define MJPEG_DIRECTORY_PATH "/mjpeg"

// 1 = after finishing all MJPEG files in the folder, start again from the first file.
// 0 = play the folder once, then stop on a "Playback complete" screen.
#define MJPEG_LOOP_FOREVER 1

// Delay in milliseconds before retrying after missing SD card, missing folder, or no playable movies.
#define MJPEG_RETRY_DELAY_MS 1000

// 1 = allow shaking the device to skip to the next video.
// 0 = disable shake-to-next-video.
#define SHAKE_NEXT_VIDEO_ENABLED 1

// ---------------------------------------------------------------------------
// Advanced App Configuration
// Change these only if you are tuning performance, memory usage, or IMU behavior.
// ---------------------------------------------------------------------------

// Size of the MJPEG decode/read buffer in bytes. Larger values can help with bigger frames but use more RAM/PSRAM.
#define MJPEG_BUFFER_SIZE_BYTES (768 * 1024)

// Shake-to-next-video detection tuning for the IMU.
// Adjust these only if you want the shake gesture to be more or less sensitive.
// IMU polling interval in milliseconds.
#define SHAKE_NEXT_VIDEO_POLL_INTERVAL_MS 20
// Ignore motion for this long after startup so initialization movement does not trigger a skip.
#define SHAKE_NEXT_VIDEO_STARTUP_SETTLE_MS 1200
// Minimum time between accepted shake-to-skip events.
#define SHAKE_NEXT_VIDEO_COOLDOWN_MS 900
// Time window in milliseconds in which multiple shake pulses must occur.
#define SHAKE_NEXT_VIDEO_CONFIRM_WINDOW_MS 260
// Number of qualifying shake pulses required before skipping to the next video.
#define SHAKE_NEXT_VIDEO_REQUIRED_PULSES 2
// Minimum accelerometer delta in g between samples to count as a shake pulse.
#define SHAKE_NEXT_VIDEO_ACCEL_DELTA_G 0.80f
// Minimum gyroscope magnitude in degrees/sec to count as a shake pulse.
#define SHAKE_NEXT_VIDEO_GYRO_DPS 150.0f

// Debounce time in milliseconds for the next-video button input.
#define NEXT_VIDEO_BUTTON_DEBOUNCE_MS 40

// ---------------------------------------------------------------------------
// Advanced Hardware Configuration
// Board-specific pins, panel timings, bus settings, and sensor wiring.
// Change only if you are porting the sketch to different hardware.
// ---------------------------------------------------------------------------

#define LCD_ROTATION 0
#define LCD_H_RES 320
#define LCD_V_RES 820

// Native IDF backend timings (manufacturer profile).
#define IDF_LCD_PCLK_HZ 16000000
#define IDF_LCD_HSYNC_FRONT_PORCH 30
#define IDF_LCD_HSYNC_PULSE_WIDTH 6
#define IDF_LCD_HSYNC_BACK_PORCH 30
#define IDF_LCD_VSYNC_FRONT_PORCH 20
#define IDF_LCD_VSYNC_PULSE_WIDTH 40
#define IDF_LCD_VSYNC_BACK_PORCH 20
#define IDF_LCD_PCLK_ACTIVE_NEG 0

// ST7701 RGBCTRL(C3h) and host timing polarity alignment for native IDF path.
#define IDF_LCD_HSYNC_ACTIVE_HIGH 0
#define IDF_LCD_VSYNC_ACTIVE_HIGH 0
#define IDF_LCD_DE_ACTIVE_HIGH 1

// Release 3-wire panel IO after init so overlapped pins can be reused (e.g. SDMMC).
#define IDF_LCD_RELEASE_PANEL_IO_AFTER_INIT 1

// Backed off for stability (reduce flicker on some boards/panels).
#define LCD_PCLK_HZ 16000000
#define LCD_HSYNC_POLARITY 1

// Tuned for smoother refresh at 16 MHz while keeping sync widths conservative.
#define LCD_HSYNC_FRONT_PORCH 16
#define LCD_HSYNC_PULSE_WIDTH 6
#define LCD_HSYNC_BACK_PORCH 16
#define LCD_VSYNC_POLARITY 1
#define LCD_VSYNC_FRONT_PORCH 10
#define LCD_VSYNC_PULSE_WIDTH 40
#define LCD_VSYNC_BACK_PORCH 10
#define LCD_PCLK_ACTIVE_NEG 0

#define LCD_PIN_SPI_CS 0
#define LCD_PIN_SPI_SCK 2
#define LCD_PIN_SPI_SDO 1

#define LCD_PIN_DE 40
#define LCD_PIN_PCLK 41
#define LCD_PIN_VSYNC 39
#define LCD_PIN_HSYNC 38
#define LCD_PIN_RESET 16

#define LCD_PIN_R0 17
#define LCD_PIN_R1 46
#define LCD_PIN_R2 3
#define LCD_PIN_R3 8
#define LCD_PIN_R4 18

#define LCD_PIN_G0 14
#define LCD_PIN_G1 13
#define LCD_PIN_G2 12
#define LCD_PIN_G3 11
#define LCD_PIN_G4 10
#define LCD_PIN_G5 9

#define LCD_PIN_B0 21
#define LCD_PIN_B1 5
#define LCD_PIN_B2 45
#define LCD_PIN_B3 48
#define LCD_PIN_B4 47

#define LCD_PIN_BACKLIGHT 6

#define LCD_BACKLIGHT_PWM_FREQ_HZ 50000
#define LCD_BACKLIGHT_DEFAULT_BRIGHTNESS 255

#define SDMMC_PIN_CLK 1
#define SDMMC_PIN_CMD 2
#define SDMMC_PIN_D0 42

// SDMMC bus speed in kHz. 20000 = default speed, usually more stable than 40000.
#define SDMMC_FREQ_KHZ 20000
#define SDMMC_REMOUNT_DELAY_MS 50

#define NEXT_VIDEO_BUTTON_PIN LCD_PIN_SPI_CS
#define NEXT_VIDEO_BUTTON_ACTIVE_LOW 1

#define SHAKE_NEXT_VIDEO_QMI8658_SDA 15
#define SHAKE_NEXT_VIDEO_QMI8658_SCL 7
#define SHAKE_NEXT_VIDEO_QMI8658_ADDRESS 0x6B
