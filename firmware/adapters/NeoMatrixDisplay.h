#pragma once

#include <stdint.h>
#include <vector>
#include "interfaces/BaseDisplay.h"

class FastLED_NeoMatrix;
struct CRGB;

class NeoMatrixDisplay : public BaseDisplay
{
public:
    NeoMatrixDisplay();
    ~NeoMatrixDisplay() override;

    bool initialize() override;
    void clear() override;
    void displayFlights(const std::vector<FlightInfo> &flights) override;
    void displayMessage(const String &message);
    void showLoading();

private:
    FastLED_NeoMatrix *_matrix = nullptr;
    CRGB *_leds = nullptr;

    uint16_t _matrixWidth  = 0;
    uint16_t _matrixHeight = 0;
    uint32_t _numPixels    = 0;

    size_t        _currentFlightIndex = 0;
    unsigned long _lastCycleMs        = 0;

    void     drawOuterBorder(uint16_t color);
    void     drawTextLine(int16_t x, int16_t y, const String &text,
                          uint16_t color, uint8_t size = 1);
    void     drawFlightText(int16_t x, int16_t y, int maxW,
                            const FlightInfo &f, uint16_t color);
    String   truncate(const String &text, int maxCols, int charW);
    void     displaySingleFlightCard(const FlightInfo &f);
    void     displayLoadingScreen();

    // Telemetry string builders
    String   buildTelemetryLine1(const FlightInfo &f);  // Alt + Speed
    String   buildTelemetryLine2(const FlightInfo &f);  // Track + Vertical rate

    void     drawLogo(int16_t x, int16_t y, const std::vector<uint16_t> &pixels);
    void     drawAirplaneIcon(int16_t x, int16_t y, uint16_t color);
    static CRGB rgb565ToCRGB(uint16_t rgb565);
};
