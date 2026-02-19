#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <vector>
#include "config/APIConfiguration.h"
#include "models/FlightInfo.h"  // for AirlineLogo::WIDTH / HEIGHT

class FlightWallFetcher
{
public:
    FlightWallFetcher() = default;
    ~FlightWallFetcher() = default;

    bool getAirlineName(const String &airlineIcao, String &outDisplayNameFull);

    bool getAircraftName(const String &aircraftIcao,
                         String &outDisplayNameShort,
                         String &outDisplayNameFull);

    // Fetches a pre-dithered RGB565 bitmap from the FlightWall CDN for the
    // given airline ICAO code. The CDN is expected to serve a binary blob of
    // exactly AirlineLogo::WIDTH * AirlineLogo::HEIGHT * 2 bytes (little-endian
    // uint16_t pixels in row-major order). Returns true and populates
    // outPixels on success; returns false and leaves outPixels empty on any
    // error (network failure, 404, wrong size, etc.).
    bool getAirlineLogo(const String &airlineIcao,
                        std::vector<uint16_t> &outPixels);

private:
    bool httpGetJson(const String &url, String &outPayload);
    bool httpGetBinary(const String &url, std::vector<uint8_t> &outBytes);
};
