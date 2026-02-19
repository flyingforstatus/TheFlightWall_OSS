/*
Purpose: Look up human-friendly airline/aircraft names and logos from FlightWall CDN.
Responsibilities:
- HTTPS GET small JSON blobs for airline/aircraft codes and parse display names.
- HTTPS GET binary RGB565 logo blobs for airline ICAO codes.
- Provide helpers used by FlightDataFetcher for user-facing labels and logos.
Inputs: Airline ICAO code or aircraft ICAO type.
Outputs: Display name strings (short/full) or RGB565 pixel vector via out parameters.
*/
#include "adapters/FlightWallFetcher.h"

bool FlightWallFetcher::httpGetJson(const String &url, String &outPayload)
{
    WiFiClientSecure client;
    if (APIConfiguration::FLIGHTWALL_INSECURE_TLS)
    {
        client.setInsecure();
    }

    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Accept", "application/json");

    int code = http.GET();
    if (code != 200)
    {
        http.end();
        return false;
    }
    outPayload = http.getString();
    http.end();
    return true;
}

bool FlightWallFetcher::httpGetBinary(const String &url, std::vector<uint8_t> &outBytes)
{
    WiFiClientSecure client;
    if (APIConfiguration::FLIGHTWALL_INSECURE_TLS)
    {
        client.setInsecure();
    }

    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Accept", "application/octet-stream");

    int code = http.GET();
    if (code != 200)
    {
        Serial.printf("FlightWallFetcher: Binary GET failed, code %d, url: %s\n", code, url.c_str());
        http.end();
        return false;
    }

    // Read response body as raw bytes via the underlying stream.
    // http.getSize() returns -1 for chunked transfer; we cap at a safe max.
    int contentLength = http.getSize();
    const size_t expectedBytes = (size_t)AirlineLogo::WIDTH * AirlineLogo::HEIGHT * 2;

    if (contentLength > 0 && (size_t)contentLength != expectedBytes)
    {
        Serial.printf("FlightWallFetcher: Logo size mismatch. Expected %u bytes, got %d\n",
                      (unsigned)expectedBytes, contentLength);
        http.end();
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    if (!stream)
    {
        http.end();
        return false;
    }

    outBytes.reserve(expectedBytes);
    uint8_t buf[64];
    size_t totalRead = 0;
    unsigned long deadline = millis() + 5000; // 5s timeout
    while (totalRead < expectedBytes && millis() < deadline)
    {
        size_t remaining = expectedBytes - totalRead;
        size_t toRead = min(remaining, sizeof(buf));
        int n = stream->readBytes(buf, toRead);
        if (n > 0)
        {
            outBytes.insert(outBytes.end(), buf, buf + n);
            totalRead += n;
        }
    }

    http.end();

    if (totalRead != expectedBytes)
    {
        Serial.printf("FlightWallFetcher: Logo read incomplete. Got %u of %u bytes\n",
                      (unsigned)totalRead, (unsigned)expectedBytes);
        outBytes.clear();
        return false;
    }

    return true;
}

bool FlightWallFetcher::getAirlineName(const String &airlineIcao, String &outDisplayNameFull)
{
    outDisplayNameFull = String("");
    if (airlineIcao.length() == 0)
        return false;

    String url = String(APIConfiguration::FLIGHTWALL_CDN_BASE_URL) + "/oss/lookup/airline/" + airlineIcao + ".json";
    String payload;
    if (!httpGetJson(url, payload))
        return false;

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err)
        return false;

    if (doc.containsKey("display_name_full"))
    {
        outDisplayNameFull = String(doc["display_name_full"].as<const char *>());
        return outDisplayNameFull.length() > 0;
    }
    return false;
}

bool FlightWallFetcher::getAircraftName(const String &aircraftIcao,
                                        String &outDisplayNameShort,
                                        String &outDisplayNameFull)
{
    outDisplayNameShort = String("");
    outDisplayNameFull = String("");
    if (aircraftIcao.length() == 0)
        return false;

    String url = String(APIConfiguration::FLIGHTWALL_CDN_BASE_URL) + "/oss/lookup/aircraft/" + aircraftIcao + ".json";
    String payload;
    if (!httpGetJson(url, payload))
        return false;

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err)
        return false;

    if (doc.containsKey("display_name_short"))
    {
        outDisplayNameShort = String(doc["display_name_short"].as<const char *>());
    }
    if (doc.containsKey("display_name_full"))
    {
        outDisplayNameFull = String(doc["display_name_full"].as<const char *>());
    }
    return outDisplayNameShort.length() > 0 || outDisplayNameFull.length() > 0;
}

bool FlightWallFetcher::getAirlineLogo(const String &airlineIcao,
                                       std::vector<uint16_t> &outPixels)
{
    outPixels.clear();
    if (airlineIcao.length() == 0)
        return false;

    // CDN URL pattern mirrors the existing airline/aircraft lookup convention.
    // The CDN serves a raw binary blob: WIDTH*HEIGHT uint16_t pixels,
    // little-endian RGB565, row-major. When the CDN returns 404 the airline
    // simply has no logo yet; that is not an error worth logging loudly.
    String url = String(APIConfiguration::FLIGHTWALL_CDN_BASE_URL) +
                 "/oss/lookup/airline/" + airlineIcao +
                 "/logo_" + String(AirlineLogo::WIDTH) + "x" + String(AirlineLogo::HEIGHT) + ".bin";

    Serial.printf("FlightWallFetcher: Fetching logo for %s\n", airlineIcao.c_str());

    std::vector<uint8_t> rawBytes;
    if (!httpGetBinary(url, rawBytes))
        return false;

    // Re-interpret raw bytes as little-endian uint16_t RGB565 pixels.
    const size_t pixelCount = (size_t)AirlineLogo::WIDTH * AirlineLogo::HEIGHT;
    outPixels.resize(pixelCount);
    for (size_t i = 0; i < pixelCount; ++i)
    {
        outPixels[i] = (uint16_t)(rawBytes[i * 2]) | ((uint16_t)(rawBytes[i * 2 + 1]) << 8);
    }

    Serial.printf("FlightWallFetcher: Logo fetched OK for %s (%u pixels)\n",
                  airlineIcao.c_str(), (unsigned)pixelCount);
    return true;
}
