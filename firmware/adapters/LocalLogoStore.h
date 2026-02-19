#pragma once
/*
Purpose: Load pre-converted airline logo bitmaps from LittleFS.
File layout: /logos/{ICAO_UPPERCASE}.bin
             Each file is exactly AirlineLogo::WIDTH * AirlineLogo::HEIGHT * 2 bytes
             of little-endian RGB565 pixels in row-major order.
             Transparent pixels are encoded as 0xF81F (pure magenta).
Usage: Call initialize() once in setup(). Then call getAirlineLogo() per flight.
       Missing logos (airline not in the set) return false gracefully — the
       display falls back to the text-only card layout automatically.
*/
#include <Arduino.h>
#include <vector>
#include "interfaces/BaseLogoStore.h"

class LocalLogoStore : public BaseLogoStore
{
public:
    LocalLogoStore() = default;
    ~LocalLogoStore() override = default;

    // Mount LittleFS. Must be called once in setup() before getAirlineLogo().
    // Returns false if the filesystem cannot be mounted (e.g., not formatted).
    bool initialize();

    // Load logo pixels for the given airline ICAO code from LittleFS.
    // airlineIcao is case-insensitive; internally uppercased before lookup.
    bool getAirlineLogo(const String &airlineIcao,
                        std::vector<uint16_t> &outPixels) override;

private:
    bool _mounted = false;
};
