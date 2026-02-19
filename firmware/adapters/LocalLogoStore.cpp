/*
Purpose: Load pre-converted airline logo bitmaps from LittleFS.
Responsibilities:
- Mount LittleFS once at startup.
- Open /logos/{ICAO}.bin, validate exact expected byte count, read into vector.
- Return false silently for missing airlines (404-equivalent).
*/
#include "adapters/LocalLogoStore.h"
#include "models/FlightInfo.h"
#include <LittleFS.h>

bool LocalLogoStore::initialize()
{
    if (!LittleFS.begin(false))  // false = don't format if not found
    {
        Serial.println("LocalLogoStore: LittleFS mount failed. "
                       "Run 'pio run --target uploadfs' to upload logo data.");
        _mounted = false;
        return false;
    }
    _mounted = true;
    Serial.println("LocalLogoStore: LittleFS mounted OK.");

    // Log total/used space for diagnostics.
    size_t total = LittleFS.totalBytes();
    size_t used  = LittleFS.usedBytes();
    Serial.printf("LocalLogoStore: Filesystem %u bytes total, %u used (%.1f%%)\n",
                  (unsigned)total, (unsigned)used,
                  total > 0 ? (used * 100.0f / total) : 0.0f);
    return true;
}

bool LocalLogoStore::getAirlineLogo(const String &airlineIcao,
                                    std::vector<uint16_t> &outPixels)
{
    outPixels.clear();

    if (!_mounted || airlineIcao.length() == 0)
        return false;

    // Uppercase the ICAO code to match filenames written by the build tool.
    String icao = airlineIcao;
    icao.toUpperCase();

    String path = String("/logos/") + icao + ".bin";

    if (!LittleFS.exists(path))
    {
        // Not an error — this airline simply has no logo bundled.
        Serial.printf("LocalLogoStore: No logo for %s (path: %s)\n",
                      icao.c_str(), path.c_str());
        return false;
    }

    File f = LittleFS.open(path, "r");
    if (!f)
    {
        Serial.printf("LocalLogoStore: Failed to open %s\n", path.c_str());
        return false;
    }

    const size_t expectedBytes = (size_t)AirlineLogo::WIDTH * AirlineLogo::HEIGHT * 2;
    size_t fileSize = f.size();

    if (fileSize != expectedBytes)
    {
        Serial.printf("LocalLogoStore: Size mismatch for %s: got %u, expected %u\n",
                      path.c_str(), (unsigned)fileSize, (unsigned)expectedBytes);
        f.close();
        return false;
    }

    // Read all bytes in one shot — file is small (1152 bytes).
    const size_t pixelCount = (size_t)AirlineLogo::WIDTH * AirlineLogo::HEIGHT;
    outPixels.resize(pixelCount);

    size_t bytesRead = f.read(reinterpret_cast<uint8_t *>(outPixels.data()), expectedBytes);
    f.close();

    if (bytesRead != expectedBytes)
    {
        Serial.printf("LocalLogoStore: Short read for %s: got %u bytes\n",
                      path.c_str(), (unsigned)bytesRead);
        outPixels.clear();
        return false;
    }

    // Re-interpret raw bytes as little-endian uint16_t (matches how the
    // Python build tool writes them with struct.pack('<H', pixel)).
    // On a little-endian platform (ESP32 is LE) the in-place cast above is
    // already correct — no byte-swapping needed.

    Serial.printf("LocalLogoStore: Loaded logo for %s (%u pixels)\n",
                  icao.c_str(), (unsigned)pixelCount);
    return true;
}
