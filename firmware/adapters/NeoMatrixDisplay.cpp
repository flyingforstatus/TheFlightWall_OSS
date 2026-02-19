/*
Purpose: Render flight info on a 128x64 WS2812B NeoPixel matrix (FlightWall Mini layout).
Layout per flight card:
  ┌──────────────────────────────────────────────────────────────────────────────────┐
  │                                                                                  │
  │   ████████   American                               (size-1, 8px)               │
  │   ████████                                                                       │
  │   32x32      TUS-LAX                               (size-2, 16px)               │
  │   logo                                                                           │
  │   ████████   CRJ700                                (size-2, 16px)               │
  │   ████████                                                                       │
  │              Alt:655m,Spd:316kph                   (size-1, full width)         │
  │              Trk:263deg,Vr:-18kph                  (size-1, full width)         │
  │                                                                                  │
  └──────────────────────────────────────────────────────────────────────────────────┘
  Outer border only: 1px white, 1px gap to all content.
  No inner box borders, no divider line.
Inputs: FlightInfo list (with sv_* telemetry fields and optional airline_logo_rgb565).
*/
#include "adapters/NeoMatrixDisplay.h"

#include <Adafruit_GFX.h>
#include <FastLED_NeoMatrix.h>
#include <FastLED.h>
#include <math.h>
#include "config/UserConfiguration.h"
#include "config/HardwareConfiguration.h"
#include "config/TimingConfiguration.h"
#include "models/FlightInfo.h"

// Transparent sentinel: pure magenta in RGB565.
static constexpr uint16_t kTransparentRGB565 = 0xF81F;

// Default GFX font metrics (size 1).
static constexpr int kCharW1 = 6;
static constexpr int kCharH1 = 8;

// Size-2 font metrics (setTextSize(2)).
static constexpr int kCharW2 = 12;
static constexpr int kCharH2 = 16;

// ── Border + gap ─────────────────────────────────────────────────────────────
static constexpr int kOuterBorder = 1;
static constexpr int kGap         = 1;
static constexpr int kInset       = kOuterBorder + kGap;  // 2

// ── Logo column ───────────────────────────────────────────────────────────────
// 1px gap between logo right edge and text — no box border needed.
static constexpr int kLogoGap  = 1;
static constexpr int kLogoColW = AirlineLogo::WIDTH + kLogoGap;  // 33px

// ── Vertical layout within content area (60px for 64px panel) ────────────────
// Content starts at kInset (y=2). 1px top padding centres the 58px layout in 60px.
// Offsets are relative to cY + 1 (= y=3).
static constexpr int kLine1OffsetY = 0;   // Airline name  (size-1, 8px)
static constexpr int kLine2OffsetY = 9;   // Route IATA    (size-2, 16px)  [+1px gap]
static constexpr int kLine3OffsetY = 25;  // Aircraft type (size-2, 16px)
static constexpr int kLine4OffsetY = 43;  // Telemetry 1   (size-1, 8px)  [+2px gap after line3]
static constexpr int kLine5OffsetY = 51;  // Telemetry 2   (size-1, 8px)
// Last pixel: cY+1 + kLine5OffsetY + kCharH1 - 1 = 2+1+51+7 = 61 = cY+cH-1. Fits exactly.

// ─────────────────────────────────────────────────────────────────────────────

NeoMatrixDisplay::NeoMatrixDisplay() {}

NeoMatrixDisplay::~NeoMatrixDisplay()
{
    if (_leds)   { delete[] _leds;   _leds   = nullptr; }
    if (_matrix) { delete   _matrix; _matrix = nullptr; }
}

bool NeoMatrixDisplay::initialize()
{
    _matrixWidth  = HardwareConfiguration::DISPLAY_MATRIX_WIDTH;
    _matrixHeight = HardwareConfiguration::DISPLAY_MATRIX_HEIGHT;
    _numPixels    = (uint32_t)_matrixWidth * (uint32_t)_matrixHeight;

    _leds = new CRGB[_numPixels];

    _matrix = new FastLED_NeoMatrix(
        _leds,
        HardwareConfiguration::DISPLAY_TILE_PIXEL_W,
        HardwareConfiguration::DISPLAY_TILE_PIXEL_H,
        HardwareConfiguration::DISPLAY_TILES_X,
        HardwareConfiguration::DISPLAY_TILES_Y,
        NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT +
            NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG +
            NEO_TILE_TOP + NEO_TILE_RIGHT + NEO_TILE_COLUMNS + NEO_TILE_ZIGZAG);

    FastLED.addLeds<WS2812B, HardwareConfiguration::DISPLAY_PIN, GRB>(_leds, _numPixels);
    _matrix->setTextWrap(false);
    _matrix->setTextSize(1);
    _matrix->setBrightness(UserConfiguration::DISPLAY_BRIGHTNESS);
    clear();
    _currentFlightIndex = 0;
    _lastCycleMs = millis();
    return true;
}

void NeoMatrixDisplay::clear()
{
    if (_matrix) { _matrix->fillScreen(0); FastLED.show(); }
}

// ── Outer border ──────────────────────────────────────────────────────────────

void NeoMatrixDisplay::drawOuterBorder(uint16_t color)
{
    for (int t = 0; t < kOuterBorder; ++t)
        _matrix->drawRect(t, t, _matrixWidth - 2*t, _matrixHeight - 2*t, color);
}

// ── Fallback airplane icon ────────────────────────────────────────────────────
// Top-down silhouette, 32×32, rendered in the text colour when no logo is available.
// 1 = lit pixel, 0 = transparent (off).
static const uint8_t kAirplaneIcon[32][32] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0},
    {0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0},
    {0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},
    {0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0},
    {0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

void NeoMatrixDisplay::drawAirplaneIcon(int16_t originX, int16_t originY, uint16_t color)
{
    for (uint8_t row = 0; row < 32; ++row)
        for (uint8_t col = 0; col < 32; ++col)
            if (kAirplaneIcon[row][col])
                _matrix->drawPixel(originX + col, originY + row, color);
}

// ── Logo ──────────────────────────────────────────────────────────────────────

CRGB NeoMatrixDisplay::rgb565ToCRGB(uint16_t rgb565)
{
    uint8_t r5 = (rgb565 >> 11) & 0x1F;
    uint8_t g6 = (rgb565 >>  5) & 0x3F;
    uint8_t b5 =  rgb565        & 0x1F;
    return CRGB((r5 << 3) | (r5 >> 2),
                (g6 << 2) | (g6 >> 4),
                (b5 << 3) | (b5 >> 2));
}

void NeoMatrixDisplay::drawLogo(int16_t originX, int16_t originY,
                                const std::vector<uint16_t> &pixels)
{
    if (pixels.size() != (size_t)AirlineLogo::WIDTH * AirlineLogo::HEIGHT)
        return;
    for (uint8_t row = 0; row < AirlineLogo::HEIGHT; ++row)
        for (uint8_t col = 0; col < AirlineLogo::WIDTH; ++col)
        {
            uint16_t rgb565 = pixels[row * AirlineLogo::WIDTH + col];
            if (rgb565 == kTransparentRGB565) continue;
            int16_t px = originX + col, py = originY + row;
            if (px < 0 || px >= (int16_t)_matrixWidth  ||
                py < 0 || py >= (int16_t)_matrixHeight) continue;
            _matrix->drawPixel(px, py, rgb565);
        }
}

// ── Text helpers ──────────────────────────────────────────────────────────────

void NeoMatrixDisplay::drawTextLine(int16_t x, int16_t y, const String &text,
                                    uint16_t color, uint8_t size)
{
    _matrix->setTextSize(size);
    _matrix->setCursor(x, y);
    _matrix->setTextColor(color);
    for (size_t i = 0; i < (size_t)text.length(); ++i)
        _matrix->write(text[i]);
    _matrix->setTextSize(1); // restore default
}

String NeoMatrixDisplay::truncate(const String &text, int maxCols, int charW)
{
    int maxChars = maxCols > 0 ? maxCols : (int)(_matrixWidth / charW);
    if ((int)text.length() <= maxChars) return text;
    if (maxChars <= 3) return text.substring(0, maxChars);
    return text.substring(0, maxChars - 3) + "...";
}

// ── Telemetry string builders ─────────────────────────────────────────────────

// "Alt:42,000,Spd:567"
String NeoMatrixDisplay::buildTelemetryLine1(const FlightInfo &f)
{
    String s = "";
    if (!isnan(f.sv_baro_altitude))
    {
        int ft = (int)round(f.sv_baro_altitude * 3.28084);
        // Format with comma separator (e.g. 42000 → "42,000")
        String ftStr = "";
        if (ft >= 1000)
        {
            ftStr  = String(ft / 1000);
            ftStr += ",";
            int remainder = ft % 1000;
            if (remainder < 100) ftStr += "0";
            if (remainder < 10)  ftStr += "0";
            ftStr += String(remainder);
        }
        else
        {
            ftStr = String(ft);
        }
        s += "Alt:";
        s += ftStr;
    }
    if (!isnan(f.sv_velocity))
    {
        if (s.length()) s += "|";
        int mph = (int)round(f.sv_velocity * 2.23694);
        s += "Spd:";
        s += String(mph);
    }
    return s.length() ? s : String("No telemetry");
}

// "Trk:263deg,Vr:-11"
String NeoMatrixDisplay::buildTelemetryLine2(const FlightInfo &f)
{
    String s = "";
    if (!isnan(f.sv_heading))
    {
        s += "Trk:";
        s += String((int)round(f.sv_heading));
        s += "deg";
    }
    if (!isnan(f.sv_vertical_rate))
    {
        if (s.length()) s += "|";
        int vr_mph = (int)round(f.sv_vertical_rate * 2.23694);
        s += "Vr:";
        s += String(vr_mph);
    }
    return s.length() ? s : String("");
}

// ── Flight card ───────────────────────────────────────────────────────────────

void NeoMatrixDisplay::displaySingleFlightCard(const FlightInfo &f)
{
    const uint16_t color = _matrix->Color(UserConfiguration::TEXT_COLOR_R,
                                          UserConfiguration::TEXT_COLOR_G,
                                          UserConfiguration::TEXT_COLOR_B);
    const uint16_t white = _matrix->Color(255, 255, 255);

    // ── Outer border only ─────────────────────────────────────────────────────
    drawOuterBorder(white);

    // ── Content area ─────────────────────────────────────────────────────────
    // All content sits within kInset (2px) of the panel edge.
    // 1px top padding centres the 58px text layout within the 60px content height.
    const int     cX    = kInset;                   // x=2
    const int     cY    = kInset;                   // y=2
    const int     cW    = _matrixWidth  - 2*kInset; // 124px
    const int     cH    = _matrixHeight - 2*kInset; // 60px
    const int16_t textY = cY + 1;                   // y=3 (1px top pad)

    // Shared vertical positioning for logo/icon: centred within the top text block.
    const int16_t topBlockTop = textY;
    const int16_t topBlockH   = kLine3OffsetY + kCharH2;  // 41px
    const int16_t iconY       = topBlockTop + (topBlockH - AirlineLogo::HEIGHT) / 2;

    if (!f.airline_logo_rgb565.empty())
        drawLogo(cX, iconY, f.airline_logo_rgb565);
    else
        drawAirplaneIcon(cX, iconY, _matrix->Color(0, 100, 255));

    // Text always in the right column, same width regardless of logo or icon.
    drawFlightText(cX + kLogoColW, textY, cW - kLogoColW, f, color);
}

// Draws all 5 text lines starting at (x, y) within maxW pixels.
void NeoMatrixDisplay::drawFlightText(int16_t x, int16_t y, int maxW,
                                      const FlightInfo &f, uint16_t color)
{
    // Line 1: Airline name (size-1)
    String airline = f.airline_display_name_full.length() ? f.airline_display_name_full
                   : (f.operator_iata.length() ? f.operator_iata
                   : (f.operator_icao.length() ? f.operator_icao : f.operator_code));
    drawTextLine(x, y + kLine1OffsetY,
                 truncate(airline, maxW / kCharW1, kCharW1), color, 1);

    // Line 2: Route IATA (size-2)
    String orig = f.origin.code_iata.length()      ? f.origin.code_iata      : f.origin.code_icao;
    String dest = f.destination.code_iata.length() ? f.destination.code_iata : f.destination.code_icao;
    drawTextLine(x, y + kLine2OffsetY,
                 truncate(orig + "-" + dest, maxW / kCharW2, kCharW2), color, 2);

    // Line 3: Aircraft type (size-2)
    String acft = f.aircraft_display_name_short.length() ? f.aircraft_display_name_short
                                                         : f.aircraft_code;
    drawTextLine(x, y + kLine3OffsetY,
                 truncate(acft, maxW / kCharW2, kCharW2), color, 2);

    // Lines 4 & 5: Telemetry (size-1, full content width not maxW —
    // telemetry spans under the logo column too).
    const int16_t btX = kInset;
    const int     btW = _matrixWidth - 2*kInset;
    drawTextLine(btX, y + kLine4OffsetY,
                 truncate(buildTelemetryLine1(f), btW / kCharW1, kCharW1), color, 1);
    drawTextLine(btX, y + kLine5OffsetY,
                 truncate(buildTelemetryLine2(f), btW / kCharW1, kCharW1), color, 1);
}

// ── Public display interface ──────────────────────────────────────────────────

void NeoMatrixDisplay::displayFlights(const std::vector<FlightInfo> &flights)
{
    if (_matrix == nullptr) return;
    _matrix->fillScreen(0);

    if (!flights.empty())
    {
        const unsigned long now        = millis();
        const unsigned long intervalMs = TimingConfiguration::DISPLAY_CYCLE_SECONDS * 1000UL;
        if (flights.size() > 1 && now - _lastCycleMs >= intervalMs)
        {
            _lastCycleMs = now;
            _currentFlightIndex = (_currentFlightIndex + 1) % flights.size();
        }
        else if (flights.size() == 1)
        {
            _currentFlightIndex = 0;
        }
        displaySingleFlightCard(flights[_currentFlightIndex % flights.size()]);
    }
    else
    {
        displayLoadingScreen();
    }
    FastLED.show();
}

void NeoMatrixDisplay::displayLoadingScreen()
{
    if (_matrix == nullptr) return;
    _matrix->fillScreen(0);
    drawOuterBorder(_matrix->Color(255, 255, 255));

    const uint16_t color = _matrix->Color(UserConfiguration::TEXT_COLOR_R,
                                          UserConfiguration::TEXT_COLOR_G,
                                          UserConfiguration::TEXT_COLOR_B);
    const String text = "...";
    const int    cW   = _matrixWidth  - 2 * kInset;
    const int    cH   = _matrixHeight - 2 * kInset;
    const int16_t x   = kInset + (cW - (int)text.length() * kCharW1) / 2;
    const int16_t y   = kInset + (cH - kCharH1) / 2;
    drawTextLine(x, y, text, color, 1);
    FastLED.show();
}

void NeoMatrixDisplay::displayMessage(const String &message)
{
    if (_matrix == nullptr) return;
    _matrix->fillScreen(0);
    drawOuterBorder(_matrix->Color(255, 255, 255));

    const uint16_t color = _matrix->Color(UserConfiguration::TEXT_COLOR_R,
                                          UserConfiguration::TEXT_COLOR_G,
                                          UserConfiguration::TEXT_COLOR_B);
    const int    cW  = _matrixWidth  - 2 * kInset;
    const int    cH  = _matrixHeight - 2 * kInset;
    const int16_t x  = kInset;
    const int16_t y  = kInset + (cH - kCharH1) / 2;
    drawTextLine(x, y, truncate(message, cW / kCharW1, kCharW1), color, 1);
    FastLED.show();
}

void NeoMatrixDisplay::showLoading() { displayLoadingScreen(); }
