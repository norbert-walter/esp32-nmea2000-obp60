#ifndef _OBP60EXTENSIONPORT_H
#define _OBP60EXTENSIONPORT_H

#include <Arduino.h>
#include "OBP60Hardware.h"
#include "LedSpiTask.h"
#include "Graphics.h"
#include <GxEPD2_BW.h>                  // GxEPD2 lib for b/w E-Ink displays
#include <Adafruit_FRAM_I2C.h>          // I2C FRAM
#include <math.h>

#ifdef TFT_DISPLAY
    #if !defined(TFT_320x480_ST7796) && !defined(TFT_320x480_ILI9488)
        #error "TFT_DISPLAY requires one panel type: TFT_320x480_ST7796 or TFT_320x480_ILI9488"
    #endif
    #if defined(TFT_320x480_ST7796) && defined(TFT_320x480_ILI9488)
        #error "Select exactly one TFT panel type: TFT_320x480_ST7796 or TFT_320x480_ILI9488"
    #endif
    #include <LovyanGFX.hpp>            // TFT LCD lib for 320x480 color displays
    #undef GxEPD_WHITE
    #define GxEPD_WHITE  TFT_WHITE      // Replacement color for white on TFT (OBPHardware.h)
    #undef GxEPD_BLACK
    #define GxEPD_BLACK  TFT_BLACK      // Replacement color for black on TFT (OBPHardware.h)
#endif

#ifdef BOARD_OBP40S3
    #include "esp_vfs_fat.h"
    #include "sdmmc_cmd.h"
    #define MOUNT_POINT "/sdcard"
#endif

// FRAM address reservations 32kB: 0x0000 - 0x7FFF
// 0x0000 - 0x03ff: single variables
#define FRAM_PAGE_NO 0x0002
#define FRAM_SYSTEM_MODE 0x009
// Voltage page
#define FRAM_VOLTAGE_AVG 0x000A
#define FRAM_VOLTAGE_TREND 0x000B
#define FRAM_VOLTAGE_MODE 0x000C
// Wind page
#define FRAM_WIND_SIZE 0x000D
#define FRAM_WIND_SRC 0x000E
#define FRAM_WIND_MODE 0x000F
// Barograph history data
#define FRAM_BAROGRAPH_START 0x0400
#define FRAM_BAROGRAPH_END 0x13FF

#define PI 3.1415926535897932384626433832795
#define EARTH_RADIUS 6371000.0

extern Adafruit_FRAM_I2C fram;
extern bool hasFRAM;
extern bool hasSDCard;
#ifdef BOARD_OBP40S3
extern sdmmc_card_t *sdcard;
#endif

// Fonts declarations for display (#includes see OBP60Extensions.cpp)
extern const GFXfont DSEG7Classic_BoldItalic10pt7b;
extern const GFXfont DSEG7Classic_BoldItalic12pt7b;
extern const GFXfont DSEG7Classic_BoldItalic16pt7b;
extern const GFXfont DSEG7Classic_BoldItalic20pt7b;
extern const GFXfont DSEG7Classic_BoldItalic26pt7b;
extern const GFXfont DSEG7Classic_BoldItalic30pt7b;
extern const GFXfont DSEG7Classic_BoldItalic42pt7b;
extern const GFXfont DSEG7Classic_BoldItalic60pt7b;
extern const GFXfont Ubuntu_Bold8pt8b;
extern const GFXfont Ubuntu_Bold10pt8b;
extern const GFXfont Ubuntu_Bold12pt8b;
extern const GFXfont Ubuntu_Bold16pt8b;
extern const GFXfont Ubuntu_Bold20pt8b;
extern const GFXfont Ubuntu_Bold32pt8b;
extern const GFXfont Atari16px;
extern const GFXfont IBM8x8px;

// Global functions
#ifdef DISPLAY_GDEW042T2
GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> & getdisplay();
#endif

#ifdef DISPLAY_GDEY042T81
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> & getdisplay();
#endif

#ifdef DISPLAY_GYE042A87
GxEPD2_BW<GxEPD2_420_GYE042A87, GxEPD2_420_GYE042A87::HEIGHT> & getdisplay();
#endif

#ifdef DISPLAY_SE0420NQ04
GxEPD2_BW<GxEPD2_420_SE0420NQ04, GxEPD2_420_SE0420NQ04::HEIGHT> & getdisplay();
#endif

#ifdef TFT_DISPLAY
// LovyanGFX based display wrapper for TFT panels
class LGFX : public lgfx::LGFX_Device {
public:
  lgfx::Bus_SPI _bus_instance;

  #ifdef TFT_320x480_ST7796
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 80000000;    // High speed ST7796
      cfg.freq_read  = 16000000;
      cfg.pin_sclk = OBP_SPI_CLK;
      cfg.pin_mosi = OBP_SPI_DIN;
      cfg.pin_miso = -1;
      cfg.pin_dc   = OBP_SPI_DC;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs  = OBP_SPI_CS;
      cfg.pin_rst = OBP_SPI_RST;
      cfg.pin_busy = -1;
      cfg.panel_width  = 320;       // Native width resolution
      cfg.panel_height = 480;       // Native hight resolution
      cfg.offset_x     = 0;         // No panel offset: full framebuffer mapping
      cfg.offset_y     = 0;         // No panel offset: full framebuffer mapping
      cfg.offset_rotation = 3;      // Rotate display content conter clock wise 90 deg ST7796
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.memory_width     = 320;
      cfg.memory_height    = 480;
      // cfg.pwm_control not available in this LovyanGFX version
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
      _panel_instance.config(cfg);
    }
        // No dedicated TFT PWM backlight pin configured on this board.
        // Keep backlight handling outside LovyanGFX to avoid LEDC init on invalid GPIO.
    setPanel(&_panel_instance);
        // Match Adafruit GFX cursor semantics: y coordinate is text baseline.
        setTextDatum(textdatum_t::baseline_left);
  }
  #endif

  #ifdef TFT_320x480_ILI9488
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;    // Slow speed ILI9488
      cfg.freq_read  = 16000000;
      cfg.pin_sclk = OBP_SPI_CLK;
      cfg.pin_mosi = OBP_SPI_DIN;
      cfg.pin_miso = -1;
      cfg.pin_dc   = OBP_SPI_DC;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs  = OBP_SPI_CS;
      cfg.pin_rst = OBP_SPI_RST;
      cfg.pin_busy = -1;
      cfg.panel_width  = 320;       // Native width resolution
      cfg.panel_height = 480;       // Native hight resolution
      cfg.offset_x     = 0;         // No panel offset: full framebuffer mapping
      cfg.offset_y     = 0;         // No panel offset: full framebuffer mapping
      cfg.offset_rotation = 1;      // Rotate display content clock wise 90 deg ILI9488
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.memory_width     = 320;
      cfg.memory_height    = 480;
      // cfg.pwm_control not available in this LovyanGFX version
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
      _panel_instance.config(cfg);
    }
        // No dedicated TFT PWM backlight pin configured on this board.
        // Keep backlight handling outside LovyanGFX to avoid LEDC init on invalid GPIO.
    setPanel(&_panel_instance);
        // Match Adafruit GFX cursor semantics: y coordinate is text baseline.
        setTextDatum(textdatum_t::baseline_left);
  }
  #endif

  // compatibility helpers --------------------------------------------------
    using lgfx::LGFX_Device::setFont;
    void setFont(const lgfx::IFont* font) {
        _currentAdfFont = nullptr;
        lgfx::LGFX_Device::setFont(font);
    }
    // Adafruit GFX fonts support on TFT via LovyanGFX bridge
    void setFont(const GFXfont *font) {
        if (font == nullptr) {
            _currentAdfFont = nullptr;
            lgfx::LGFX_Device::setFont(nullptr);
            return;
        }

        if (font->glyph == nullptr || font->bitmap == nullptr || font->last < font->first) {
            _currentAdfFont = nullptr;
            lgfx::LGFX_Device::setFont(nullptr);
            return;
        }

        const uint16_t glyphCount = static_cast<uint16_t>(font->last - font->first + 1);
        if (glyphCount == 0) {
            lgfx::LGFX_Device::setFont(nullptr);
            return;
        }

        if (_adfGlyphCount != glyphCount || _adfGlyphBridge == nullptr) {
            if (_adfGlyphBridge != nullptr) {
                free(_adfGlyphBridge);
                _adfGlyphBridge = nullptr;
                _adfGlyphCount = 0;
            }
            _adfGlyphBridge = static_cast<lgfx::GFXglyph*>(malloc(sizeof(lgfx::GFXglyph) * glyphCount));
            if (_adfGlyphBridge == nullptr) {
                lgfx::LGFX_Device::setFont(nullptr);
                return;
            }
            _adfGlyphCount = glyphCount;
        }

        for (uint16_t index = 0; index < glyphCount; ++index) {
            _adfGlyphBridge[index].bitmapOffset = font->glyph[index].bitmapOffset;
            _adfGlyphBridge[index].width = font->glyph[index].width;
            _adfGlyphBridge[index].height = font->glyph[index].height;
            _adfGlyphBridge[index].xAdvance = font->glyph[index].xAdvance;
            _adfGlyphBridge[index].xOffset = font->glyph[index].xOffset;
            _adfGlyphBridge[index].yOffset = font->glyph[index].yOffset;
        }

        _adfFontBridge = lgfx::GFXfont(
            const_cast<uint8_t*>(font->bitmap),
            _adfGlyphBridge,
            font->first,
            font->last,
            font->yAdvance
        );
        _currentAdfFont = font;
        lgfx::LGFX_Device::setFont(&_adfFontBridge);
    }

    void getTextBounds(const String &txt, int16_t x, int16_t y,
                                         int16_t *x0, int16_t *y0,
                                         uint16_t *w, uint16_t *h) {
        if (w == nullptr || h == nullptr) {
            return;
        }
        if (_currentAdfFont == nullptr || txt.length() == 0) {
            *w = textWidth(txt);
            *h = fontHeight();
            if (x0) *x0 = x;
            if (y0) *y0 = y - static_cast<int16_t>(*h);
            return;
        }

        const float sx = getTextSizeX();
        const float sy = getTextSizeY();

        int32_t cursorX = x;
        int32_t cursorY = y;
        int32_t minX = 0;
        int32_t minY = 0;
        int32_t maxX = 0;
        int32_t maxY = 0;
        bool hasPixel = false;

        for (size_t i = 0; i < txt.length(); ++i) {
            char c = txt[i];
            if (c == '\r') continue;
            if (c == '\n') {
                cursorX = x;
                cursorY += static_cast<int32_t>(_currentAdfFont->yAdvance * sy);
                continue;
            }
            if (c < _currentAdfFont->first || c > _currentAdfFont->last) {
                continue;
            }

            const GFXglyph* glyph = &_currentAdfFont->glyph[static_cast<uint16_t>(c) - _currentAdfFont->first];
            const int32_t gw = static_cast<int32_t>(glyph->width * sx);
            const int32_t gh = static_cast<int32_t>(glyph->height * sy);
            const int32_t gx1 = cursorX + static_cast<int32_t>(glyph->xOffset * sx);
            const int32_t gy1 = cursorY + static_cast<int32_t>(glyph->yOffset * sy);

            if (gw > 0 && gh > 0) {
                const int32_t gx2 = gx1 + gw - 1;
                const int32_t gy2 = gy1 + gh - 1;
                if (!hasPixel) {
                    minX = gx1; minY = gy1; maxX = gx2; maxY = gy2;
                    hasPixel = true;
                } else {
                    if (gx1 < minX) minX = gx1;
                    if (gy1 < minY) minY = gy1;
                    if (gx2 > maxX) maxX = gx2;
                    if (gy2 > maxY) maxY = gy2;
                }
            }
            cursorX += static_cast<int32_t>(glyph->xAdvance * sx);
        }

        if (hasPixel) {
            if (x0) *x0 = static_cast<int16_t>(minX);
            if (y0) *y0 = static_cast<int16_t>(minY);
            *w = static_cast<uint16_t>(maxX - minX + 1);
            *h = static_cast<uint16_t>(maxY - minY + 1);
        } else {
            if (x0) *x0 = x;
            if (y0) *y0 = y;
            *w = 0;
            *h = 0;
        }
    }
  // E-Ink interface compatibility
  void setFullWindow() { /* no-op on TFT */ }

    // Runtime panel offset control for TFT panels
    void setPanelOffset(int16_t x, int16_t y) {
            auto cfg = _panel_instance.config();
            cfg.offset_x = x;
            cfg.offset_y = y;
            _panel_instance.config(cfg);
    }

    void getPanelOffset(int16_t &x, int16_t &y) {
            auto cfg = _panel_instance.config();
            x = cfg.offset_x;
            y = cfg.offset_y;
    }

private:
    lgfx::GFXfont _adfFontBridge { nullptr, nullptr, 0, 0, 0 };
    lgfx::GFXglyph* _adfGlyphBridge = nullptr;
    uint16_t _adfGlyphCount = 0;
    const GFXfont* _currentAdfFont = nullptr;
    #if defined(TFT_320x480_ST7796)
    lgfx::Panel_ST7796 _panel_instance;
    #elif defined(TFT_320x480_ILI9488)
    lgfx::Panel_ILI9488 _panel_instance;
    #endif
};

class LGFXCanvas : public lgfx::LGFX_Sprite {
public:
    explicit LGFXCanvas(lgfx::LGFX_Device* parent = nullptr) : lgfx::LGFX_Sprite(parent) {}

    using lgfx::LGFX_Sprite::setFont;
    void setFont(const GFXfont *font) {
        if (font == nullptr) {
            _currentAdfFont = nullptr;
            lgfx::LGFX_Sprite::setFont(nullptr);
            return;
        }

        if (font->glyph == nullptr || font->bitmap == nullptr || font->last < font->first) {
            _currentAdfFont = nullptr;
            lgfx::LGFX_Sprite::setFont(nullptr);
            return;
        }

        const uint16_t glyphCount = static_cast<uint16_t>(font->last - font->first + 1);
        if (glyphCount == 0) {
            lgfx::LGFX_Sprite::setFont(nullptr);
            return;
        }

        if (_adfGlyphCount != glyphCount || _adfGlyphBridge == nullptr) {
            if (_adfGlyphBridge != nullptr) {
                free(_adfGlyphBridge);
                _adfGlyphBridge = nullptr;
                _adfGlyphCount = 0;
            }
            _adfGlyphBridge = static_cast<lgfx::GFXglyph*>(malloc(sizeof(lgfx::GFXglyph) * glyphCount));
            if (_adfGlyphBridge == nullptr) {
                lgfx::LGFX_Sprite::setFont(nullptr);
                return;
            }
            _adfGlyphCount = glyphCount;
        }

        for (uint16_t index = 0; index < glyphCount; ++index) {
            _adfGlyphBridge[index].bitmapOffset = font->glyph[index].bitmapOffset;
            _adfGlyphBridge[index].width = font->glyph[index].width;
            _adfGlyphBridge[index].height = font->glyph[index].height;
            _adfGlyphBridge[index].xAdvance = font->glyph[index].xAdvance;
            _adfGlyphBridge[index].xOffset = font->glyph[index].xOffset;
            _adfGlyphBridge[index].yOffset = font->glyph[index].yOffset;
        }

        _adfFontBridge = lgfx::GFXfont(
            const_cast<uint8_t*>(font->bitmap),
            _adfGlyphBridge,
            font->first,
            font->last,
            font->yAdvance
        );
        _currentAdfFont = font;
        lgfx::LGFX_Sprite::setFont(&_adfFontBridge);
    }

    void getTextBounds(const String &txt, int16_t x, int16_t y,
                       int16_t *x0, int16_t *y0,
                       uint16_t *w, uint16_t *h) {
        if (w == nullptr || h == nullptr) {
            return;
        }
        if (_currentAdfFont == nullptr || txt.length() == 0) {
            *w = textWidth(txt);
            *h = fontHeight();
            if (x0) *x0 = x;
            if (y0) *y0 = y - static_cast<int16_t>(*h);
            return;
        }

        const float sx = getTextSizeX();
        const float sy = getTextSizeY();

        int32_t cursorX = x;
        int32_t cursorY = y;
        int32_t minX = 0;
        int32_t minY = 0;
        int32_t maxX = 0;
        int32_t maxY = 0;
        bool hasPixel = false;

        for (size_t i = 0; i < txt.length(); ++i) {
            char c = txt[i];
            if (c == '\r') continue;
            if (c == '\n') {
                cursorX = x;
                cursorY += static_cast<int32_t>(_currentAdfFont->yAdvance * sy);
                continue;
            }
            if (c < _currentAdfFont->first || c > _currentAdfFont->last) {
                continue;
            }

            const GFXglyph* glyph = &_currentAdfFont->glyph[static_cast<uint16_t>(c) - _currentAdfFont->first];
            const int32_t gw = static_cast<int32_t>(glyph->width * sx);
            const int32_t gh = static_cast<int32_t>(glyph->height * sy);
            const int32_t gx1 = cursorX + static_cast<int32_t>(glyph->xOffset * sx);
            const int32_t gy1 = cursorY + static_cast<int32_t>(glyph->yOffset * sy);

            if (gw > 0 && gh > 0) {
                const int32_t gx2 = gx1 + gw - 1;
                const int32_t gy2 = gy1 + gh - 1;
                if (!hasPixel) {
                    minX = gx1; minY = gy1; maxX = gx2; maxY = gy2;
                    hasPixel = true;
                } else {
                    if (gx1 < minX) minX = gx1;
                    if (gy1 < minY) minY = gy1;
                    if (gx2 > maxX) maxX = gx2;
                    if (gy2 > maxY) maxY = gy2;
                }
            }
            cursorX += static_cast<int32_t>(glyph->xAdvance * sx);
        }

        if (hasPixel) {
            if (x0) *x0 = static_cast<int16_t>(minX);
            if (y0) *y0 = static_cast<int16_t>(minY);
            *w = static_cast<uint16_t>(maxX - minX + 1);
            *h = static_cast<uint16_t>(maxY - minY + 1);
        } else {
            if (x0) *x0 = x;
            if (y0) *y0 = y;
            *w = 0;
            *h = 0;
        }
    }

    void setFullWindow() { /* no-op on TFT */ }

private:
    lgfx::GFXfont _adfFontBridge { nullptr, nullptr, 0, 0, 0 };
    lgfx::GFXglyph* _adfGlyphBridge = nullptr;
    uint16_t _adfGlyphCount = 0;
    const GFXfont* _currentAdfFont = nullptr;
};

LGFXCanvas & getdisplay();
LGFX & getpaneldisplay();
LGFXCanvas & getscaleddisplay();
bool initDisplayShadowBuffer();
bool initDisplayScaleBuffer(uint16_t width, uint16_t height);
#endif

// Page display return values
#define PAGE_OK 0          // all ok, do nothing
#define PAGE_UPDATE 1      // page wants display to update
#define PAGE_HIBERNATE 2   // page wants displey to hibernate

#ifdef TFT_DISPLAY
#ifndef OBP_TFT_ENABLE_SCALING
#define OBP_TFT_ENABLE_SCALING 1
#endif

#ifndef OBP_TFT_SCALE_ANTIALIAS
#define OBP_TFT_SCALE_ANTIALIAS 1
#endif

#if OBP_TFT_SCALE_ANTIALIAS
inline uint16_t lerpRgb565(uint16_t c0, uint16_t c1, uint16_t w8) {
    const uint16_t r0 = (c0 >> 11) & 0x1F;
    const uint16_t g0 = (c0 >> 5)  & 0x3F;
    const uint16_t b0 = c0 & 0x1F;

    const uint16_t r1 = (c1 >> 11) & 0x1F;
    const uint16_t g1 = (c1 >> 5)  & 0x3F;
    const uint16_t b1 = c1 & 0x1F;

    const uint16_t r = static_cast<uint16_t>(r0 + ((static_cast<int32_t>(r1) - r0) * w8 + 128) / 256);
    const uint16_t g = static_cast<uint16_t>(g0 + ((static_cast<int32_t>(g1) - g0) * w8 + 128) / 256);
    const uint16_t b = static_cast<uint16_t>(b0 + ((static_cast<int32_t>(b1) - b0) * w8 + 128) / 256);

    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

inline uint16_t sampleBilinearRgb565(LGFXCanvas& src, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t wx, uint16_t wy) {
    const uint16_t c00 = src.readPixel(x0, y0);
    const uint16_t c10 = src.readPixel(x1, y0);
    const uint16_t c01 = src.readPixel(x0, y1);
    const uint16_t c11 = src.readPixel(x1, y1);

    const uint16_t top = lerpRgb565(c00, c10, wx);
    const uint16_t bot = lerpRgb565(c01, c11, wx);
    return lerpRgb565(top, bot, wy);
}
#endif
#endif

// Draw monochrome bitmap on both E-Ink and TFT displays
// supports various packing and bit orders; optional runtime conversion for TFT
inline void drawMonochromeBitmap(
    int16_t x, int16_t y,
    const uint8_t *bmp,
    int16_t w, int16_t h,
    uint16_t color,
    bool vertical=false,    // true: bytes run vertically (each byte 8 pixels down)
    bool lsbFirst=false,    // true: least significant bit = left/top pixel
    bool mirrorX=false)      // true: bytes run right-to-left within each row
{
    #ifdef TFT_DISPLAY
    // TFT converts per‑pixel
    int bytesPerRow = (w + 7) / 8;
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            int byteIdx;
            int bitIdx;
            if (vertical) {
                // vertical packing: column-major bytes
                int col = mirrorX ? (w - 1 - xx) : xx;
                byteIdx = col * ((h + 7) / 8) + (yy / 8);
                bitIdx  = yy % 8;
            } else {
                // horizontal packing: row-major bytes
                int col = mirrorX ? (w - 1 - xx) : xx;
                byteIdx = yy * bytesPerRow + (col / 8);
                bitIdx  = col % 8;
            }
            uint8_t b = bmp[byteIdx];
            bool pix;
            if (lsbFirst) {
                pix = b & (1 << bitIdx);
            } else {
                pix = b & (1 << (7 - bitIdx));
            }
            if (pix) {
                getdisplay().drawPixel(x + xx, y + yy, color);
            }
        }
        if ((yy & 0x0F) == 0) {
            yield();
        }
    }
    #else
    // E‑Paper: just hand over to driver (expects MSB‑first horizontal)
    getdisplay().drawBitmap(x, y, bmp, w, h, color);
    #endif
}


// Display wrapper functions for E-Ink/TFT compatibility

// generic bitmap draw that accepts 1‑bit data; TFT version
// forwards to drawMonochromeBitmap whereas EPD uses native drawBitmap
inline void displayDrawBitmap(int16_t x, int16_t y,
                              const uint8_t *bmp,
                              int16_t w, int16_t h,
                              uint16_t color) {
    #ifdef TFT_DISPLAY
    drawMonochromeBitmap(x, y, bmp, w, h, color);
    #else
    getdisplay().drawBitmap(x, y, bmp, w, h, color);
    #endif
}

inline void displayFirstPage() {
    #ifdef TFT_DISPLAY
    initDisplayShadowBuffer();
    #else
    getdisplay().firstPage();
    #endif
}

inline void displayNextPage() {
    #ifdef TFT_DISPLAY
    if (initDisplayShadowBuffer()) {
        LGFXCanvas &src = getdisplay();
        LGFX &dst = getpaneldisplay();

        const uint16_t srcW = GxEPD_WIDTH;
        const uint16_t srcH = GxEPD_HEIGHT;
        const uint16_t dstW = static_cast<uint16_t>(dst.width());
        const uint16_t dstH = static_cast<uint16_t>(dst.height());

        #if !OBP_TFT_ENABLE_SCALING
        const uint16_t drawX = static_cast<uint16_t>((dstW > srcW) ? ((dstW - srcW) / 2U) : 0U);
        const uint16_t drawY = static_cast<uint16_t>((dstH > srcH) ? ((dstH - srcH) / 2U) : 0U);
        src.pushSprite(drawX, drawY);
        #else

        const uint16_t targetH = (dstH < 320U) ? dstH : 320U;
        const uint32_t scaledW32 = (static_cast<uint32_t>(srcW) * targetH + (srcH / 2U)) / srcH;
        const uint16_t targetW = static_cast<uint16_t>((scaledW32 < dstW) ? scaledW32 : dstW);

        const uint16_t drawX = static_cast<uint16_t>((dstW - targetW) / 2U);
        const uint16_t drawY = static_cast<uint16_t>((dstH - targetH) / 2U);

        const uint16_t borderColor = src.readPixel(0, 0);
        if (initDisplayScaleBuffer(dstW, dstH)) {
            LGFXCanvas &scaled = getscaleddisplay();
            scaled.fillScreen(borderColor);

            for (uint16_t y = 0; y < targetH; ++y) {
                const uint32_t syfp = (targetH > 1)
                                        ? (static_cast<uint32_t>(y) * (srcH - 1) * 256U) / (targetH - 1)
                                        : 0;
                const uint16_t sy0 = static_cast<uint16_t>(syfp >> 8);
                const uint16_t sy1 = (sy0 + 1 < srcH) ? static_cast<uint16_t>(sy0 + 1) : sy0;
                const uint16_t wy  = static_cast<uint16_t>(syfp & 0xFFU);

                for (uint16_t x = 0; x < targetW; ++x) {
                    const uint32_t sxfp = (targetW > 1)
                                            ? (static_cast<uint32_t>(x) * (srcW - 1) * 256U) / (targetW - 1)
                                            : 0;
                    const uint16_t sx0 = static_cast<uint16_t>(sxfp >> 8);
                    const uint16_t sx1 = (sx0 + 1 < srcW) ? static_cast<uint16_t>(sx0 + 1) : sx0;

                    #if OBP_TFT_SCALE_ANTIALIAS
                    const uint16_t wx = static_cast<uint16_t>(sxfp & 0xFFU);
                    const uint16_t color = sampleBilinearRgb565(src, sx0, sy0, sx1, sy1, wx, wy);
                    #else
                    const uint16_t color = src.readPixel(sx0, sy0);
                    #endif

                    scaled.drawPixel(drawX + x, drawY + y, color);
                }
                if ((y & 0x0F) == 0) {
                    yield();
                }
            }

            scaled.pushSprite(0, 0);
        } else {
            src.pushSprite(drawX, drawY);
        }
        #endif
    }
    #else
    getdisplay().nextPage();
    #endif
}

inline void displaySetPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    #ifdef TFT_DISPLAY
    // TFT LCD doesn't use partial windows
    (void)x; (void)y; (void)w; (void)h;
    #else
    getdisplay().setPartialWindow(x, y, w, h);
    #endif
}

inline void displaySetFullWindow() {
    #ifdef TFT_DISPLAY
    // TFT LCD doesn't need setFullWindow()
    #else
    getdisplay().setFullWindow();
    #endif
}

// replacement for getTextBounds that works with both EPD and TFT
inline void displayGetTextBounds(const String &txt, int16_t x, int16_t y,
                                 int16_t *x0, int16_t *y0,
                                 uint16_t *w, uint16_t *h, const bool dsegAdjust = false) {
String str = txt;
// make sure that we always have a char with max size at last position for boundary test
// for monotype italic DSEG7 font to avoid text wobbling
if (dsegAdjust && str.length() > 0 && isDigit(str[str.length() - 1])) {
    str.setCharAt(str.length() - 1, '8'); 
}
#ifdef TFT_DISPLAY
    getdisplay().getTextBounds(str, x, y, x0, y0, w, h);
#else
    getdisplay().getTextBounds(str, x, y, x0, y0, w, h);
#endif
}

void fillPoly4(const std::vector<Point>& p4, uint16_t color);
void drawPoly(const std::vector<Point>& points, uint16_t color);

void deepSleep(CommonData &common);

uint8_t getLastPage();

void hardwareInit(GwApi *api);
void powerInit(String powermode);

void setPCF8574PortPinModul1(uint8_t pin, uint8_t value);// Set PCF8574 port pin
void setPortPin(uint pin, bool value);          // Set port pin for extension port
void togglePortPin(uint pin);                   // Toggle extension port pin

Color colorMapping(const String &colorString);          // Color mapping string to CHSV colors
void setBacklightLED(uint brightness, const Color &color);// Set backlight LEDs
void toggleBacklightLED(uint brightness,const Color &color);// Toggle backlight LEDs
void stepsBacklightLED(uint brightness, const Color &color);// Set backlight LEDs in 4 steps (100%, 50%, 10%, 0%)
BacklightMode backlightMapping(const String &backlightString);// Configuration string to value

void setFlashLED(bool status);                  // Set flash LED
void blinkingFlashLED();                        // Blinking function for flash LED
void setBlinkingLED(bool on);                   // Set blinking flash LED active


void createBuzzerTask();                        // Create separate buzzer task
void buzzer(uint frequency, uint duration);     // Ring buzzer
void setBuzzerPower(uint power);                // Set buzzer power

String xdrDelete(String input);                 // Delete xdr prefix from string

int16_t drawTextCenter(int16_t cx, int16_t cy, String text, bool dsegAdjust = false);
void drawButtonCenter(int16_t cx, int16_t cy, int8_t sx, int8_t sy, String text, uint16_t fg, uint16_t bg, bool inverted);
int16_t drawTextRalign(int16_t x, int16_t y, const String& text, bool dsegAdjust = false);
void drawTextBoxed(Rect box, String text, uint16_t fg, uint16_t bg, bool inverted, bool border);
void printBoatValue(GwApi::BoatValue *bValue, CommonData &commondata, const int16_t x, const int16_t y, const int8_t align, const int8_t fontSize, const bool smallDecs = false);
void printBoatValue(const String &sBoatValue, const int16_t x, const int16_t y, const int8_t align, const int8_t fontSize, const bool smallDecs = false);

void displayTrendHigh(int16_t x, int16_t y, uint16_t size, uint16_t color);
void displayTrendLow(int16_t x, int16_t y, uint16_t size, uint16_t color);

void displayHeader(CommonData &commonData, GwApi::BoatValue *date, GwApi::BoatValue *time, GwApi::BoatValue *hdop); // Draw display header
void displayFooter(CommonData &commonData);
void displayAlarm(CommonData &commonData);

SunData calcSunsetSunrise(double time, double date, double latitude, double longitude, float timezone); // Calulate sunset and sunrise
SunData calcSunsetSunriseRTC(struct tm *rtctime, double latitude, double longitude, float timezone);

void batteryGraphic(uint x, uint y, float percent, int pcolor, int bcolor); // Battery graphic with fill level
void solarGraphic(uint x, uint y, int pcolor, int bcolor);                  // Solar graphic
void generatorGraphic(uint x, uint y, int pcolor, int bcolor);              // Generator graphic
void startLedTask(GwApi *api);

// Display rudder position as horizontal bargraph with configurable +/- range (degrees)
// 'rangeDeg' is unsigned and will be clamped to [10,45]
void displayRudderPosition(int rudderPosition, uint8_t rangeDeg, uint16_t cx, uint16_t cy, uint16_t fg, uint16_t bg);

void doImageRequest(GwApi *api, int *pageno, const PageStruct pages[MAX_PAGE_NUMBER], AsyncWebServerRequest *request);

double distanceBetweenCoordinates(double lat1, double lon1, double lat2, double lon2); // Calculate the distance between two geo coordinates

// Icons
#define icon_width 16
#define icon_height 16

static unsigned char left_bits[] PROGMEM = {
   0x00, 0x00, 0xc0, 0x01, 0xe0, 0x01, 0xf0, 0x01, 0xf8, 0x01, 0xfc, 0x7f,
   0xfe, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xfe, 0x7f, 0xfc, 0x7f, 0xf8, 0x01,
   0xf0, 0x01, 0xe0, 0x01, 0xc0, 0x01, 0x00, 0x00 };

static unsigned char right_bits[] PROGMEM = {
   0x00, 0x00, 0x80, 0x03, 0x80, 0x07, 0x80, 0x0f, 0x80, 0x1f, 0xfe, 0x3f,
   0xfe, 0x7f, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0x7f, 0xfe, 0x3f, 0x80, 0x1f,
   0x80, 0x0f, 0x80, 0x07, 0x80, 0x03, 0x00, 0x00 };

static unsigned char lock_bits[] PROGMEM = {
   0xc0, 0x03, 0x60, 0x06, 0x30, 0x0c, 0x10, 0x08, 0x10, 0x08, 0x10, 0x08,
   0xfc, 0x3f, 0x04, 0x20, 0x04, 0x20, 0x84, 0x21, 0x84, 0x21, 0x84, 0x21,
   0x04, 0x20, 0x04, 0x20, 0x04, 0x20, 0xfc, 0x3f };

static unsigned char plus_bits[] PROGMEM = {
   0x00, 0x00, 0xe0, 0x01, 0x18, 0x06, 0x04, 0x08, 0xc4, 0x08, 0xc2, 0x10,
   0xf2, 0x13, 0xf2, 0x13, 0xc2, 0x10, 0xc4, 0x08, 0x04, 0x0c, 0x18, 0x1e,
   0xe0, 0x39, 0x00, 0x70, 0x00, 0xe0, 0x00, 0xc0 };

static unsigned char minus_bits[] PROGMEM = {
   0x00, 0x00, 0xe0, 0x01, 0x18, 0x06, 0x04, 0x08, 0x04, 0x08, 0x02, 0x10,
   0xf2, 0x13, 0xf2, 0x13, 0x02, 0x10, 0x04, 0x08, 0x04, 0x0c, 0x18, 0x1e,
   0xe0, 0x39, 0x00, 0x70, 0x00, 0xe0, 0x00, 0xc0 };

static unsigned char fram_bits[] PROGMEM = {
   0xf8, 0x1f, 0xff, 0xff, 0x9f, 0xff, 0x98, 0x1f, 0xf8, 0x1f, 0xff, 0xff,
   0xff, 0xff, 0xf8, 0x1f, 0xf8, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x1f,
   0xf8, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x1f };

static unsigned char ap_bits[] PROGMEM = {
   0xe0, 0x03, 0x18, 0x0c, 0x04, 0x10, 0xc2, 0x21, 0x30, 0x06, 0x08, 0x08,
   0xc0, 0x01, 0x20, 0x02, 0x00, 0x00, 0x80, 0x00, 0xc0, 0x01, 0xc0, 0x01,
   0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00 };

static unsigned char dish_bits[] PROGMEM = {
   0x3c, 0x00, 0x42, 0x18, 0xfa, 0x1b, 0x02, 0x04, 0x02, 0x0a, 0x02, 0x09,
   0x82, 0x08, 0x06, 0x0a, 0x0e, 0x1b, 0x9c, 0x2b, 0x38, 0x2b, 0x74, 0x20,
   0xec, 0x1f, 0x1c, 0x00, 0xf4, 0x00, 0xfe, 0x03 };

static std::map<String, unsigned char *> iconmap = {
    {"LEFT", left_bits},
    {"RIGHT", right_bits},
    {"LOCK", lock_bits},
    {"PLUS", plus_bits},
    {"MINUS", minus_bits},
    {"DISH", dish_bits},
    {"AP", ap_bits}
};

// Battery
#define battery_width 24
#define battery_height 16

static unsigned char battery_0_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x0f, 0xff, 0xff, 0x1f,
   0x03, 0x00, 0x18, 0x03, 0x00, 0x78, 0x03, 0x00, 0xf8, 0x03, 0x00, 0xd8,
   0x03, 0x00, 0xd8, 0x03, 0x00, 0xd8, 0x03, 0x00, 0xf8, 0x03, 0x00, 0x78,
   0x03, 0x00, 0x18, 0xff, 0xff, 0x1f, 0xfe, 0xff, 0x0f, 0x00, 0x00, 0x00 };

static unsigned char battery_25_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x0f, 0xff, 0xff, 0x1f,
   0x03, 0x00, 0x18, 0x3b, 0x00, 0x78, 0x3b, 0x00, 0xf8, 0x3b, 0x00, 0xd8,
   0x3b, 0x00, 0xd8, 0x3b, 0x00, 0xd8, 0x3b, 0x00, 0xf8, 0x3b, 0x00, 0x78,
   0x03, 0x00, 0x18, 0xff, 0xff, 0x1f, 0xfe, 0xff, 0x0f, 0x00, 0x00, 0x00 };

static unsigned char battery_50_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x0f, 0xff, 0xff, 0x1f,
   0x03, 0x00, 0x18, 0xbb, 0x03, 0x78, 0xbb, 0x03, 0xf8, 0xbb, 0x03, 0xd8,
   0xbb, 0x03, 0xd8, 0xbb, 0x03, 0xd8, 0xbb, 0x03, 0xf8, 0xbb, 0x03, 0x78,
   0x03, 0x00, 0x18, 0xff, 0xff, 0x1f, 0xfe, 0xff, 0x0f, 0x00, 0x00, 0x00 };

static unsigned char battery_75_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x0f, 0xff, 0xff, 0x1f,
   0x03, 0x00, 0x18, 0xbb, 0x3b, 0x78, 0xbb, 0x3b, 0xf8, 0xbb, 0x3b, 0xd8,
   0xbb, 0x3b, 0xd8, 0xbb, 0x3b, 0xd8, 0xbb, 0x3b, 0xf8, 0xbb, 0x3b, 0x78,
   0x03, 0x00, 0x18, 0xff, 0xff, 0x1f, 0xfe, 0xff, 0x0f, 0x00, 0x00, 0x00 };

static unsigned char battery_100_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x0f, 0xff, 0xff, 0x1f,
   0x03, 0x00, 0x18, 0xbb, 0xbb, 0x7b, 0xbb, 0xbb, 0xfb, 0xbb, 0xbb, 0xdb,
   0xbb, 0xbb, 0xdb, 0xbb, 0xbb, 0xdb, 0xbb, 0xbb, 0xfb, 0xbb, 0xbb, 0x7b,
   0x03, 0x00, 0x18, 0xff, 0xff, 0x1f, 0xfe, 0xff, 0x0f, 0x00, 0x00, 0x00 };

static unsigned char battery_loading_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0xfe, 0xe4, 0x0f, 0xff, 0xec, 0x1f,
   0x03, 0x08, 0x18, 0x03, 0x18, 0x78, 0x03, 0x30, 0xf8, 0x83, 0x3f, 0xd8,
   0x03, 0x7f, 0xd8, 0x03, 0x03, 0xd8, 0x03, 0x06, 0xf8, 0x03, 0x04, 0x78,
   0x03, 0x0c, 0x18, 0xff, 0xcb, 0x1f, 0xfe, 0xd3, 0x0f, 0x00, 0x10, 0x00 };

// Other symbols
#define swipe_width 24
#define swipe_height 16
static unsigned char swipe_bits[] PROGMEM = {
   0x00, 0x06, 0x00, 0x24, 0x09, 0x24, 0x12, 0x09, 0x48, 0x7f, 0x09, 0xfe,
   0x12, 0xb9, 0x48, 0x24, 0xc9, 0x25, 0x40, 0x49, 0x02, 0xa0, 0x49, 0x06,
   0x20, 0x01, 0x0a, 0x20, 0x00, 0x08, 0x40, 0x00, 0x08, 0x40, 0x00, 0x08,
   0x80, 0x00, 0x04, 0x00, 0x01, 0x04, 0x00, 0x02, 0x02, 0x00, 0xfc, 0x01 };

#define exclamation_width 32
#define exclamation_height 32
static unsigned char exclamation_bits[] PROGMEM = {
   0x00, 0xc0, 0x03, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0xb0, 0x0d, 0x00,
   0x00, 0xd8, 0x1b, 0x00, 0x00, 0xec, 0x37, 0x00, 0x00, 0xf6, 0x6f, 0x00,
   0x00, 0x3b, 0xdc, 0x00, 0x80, 0x3d, 0xbc, 0x01, 0xc0, 0x3e, 0x7c, 0x03,
   0x60, 0x3f, 0xfc, 0x06, 0xb0, 0x3f, 0xfc, 0x0d, 0xd8, 0x3f, 0xfc, 0x1b,
   0xec, 0x3f, 0xfc, 0x37, 0xf6, 0x3f, 0xfc, 0x6f, 0xfb, 0x3f, 0xfc, 0xdf,
   0xfd, 0x3f, 0xfc, 0xbf, 0xfd, 0x3f, 0xfc, 0xbf, 0xfb, 0x3f, 0xfc, 0xdf,
   0xf6, 0x3f, 0xfc, 0x6f, 0xec, 0x3f, 0xfc, 0x37, 0xd8, 0xff, 0xff, 0x1b,
   0xb0, 0xff, 0xff, 0x0d, 0x60, 0x3f, 0xfc, 0x06, 0xc0, 0x3e, 0x7c, 0x03,
   0x80, 0x3d, 0xbc, 0x01, 0x00, 0x3b, 0xdc, 0x00, 0x00, 0xf6, 0x6f, 0x00,
   0x00, 0xec, 0x37, 0x00, 0x00, 0xd8, 0x1b, 0x00, 0x00, 0xb0, 0x0d, 0x00,
   0x00, 0x60, 0x06, 0x00, 0x00, 0xc0, 0x03, 0x00 };

#endif
