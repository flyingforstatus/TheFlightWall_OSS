/*
Purpose: Firmware entry point for ESP32.
Responsibilities:
- Initialize serial, mount LittleFS (logo store), connect to Wi-Fi.
- Construct fetchers, logo store, and display.
- Periodically fetch state vectors (OpenSky), enrich flights (AeroAPI), and render.
Configuration: UserConfiguration (location/filters/colors), TimingConfiguration (intervals),
               WiFiConfiguration (SSID/password), HardwareConfiguration (display specs).
Logo data:     Upload via 'pio run --target uploadfs' after running tools/build_logos.py.
*/
#include <vector>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "config/UserConfiguration.h"
#include "config/WiFiConfiguration.h"
#include "config/TimingConfiguration.h"
#include "adapters/OpenSkyFetcher.h"
#include "adapters/AeroAPIFetcher.h"
#include "adapters/LocalLogoStore.h"
#include "core/FlightDataFetcher.h"
#include "adapters/NeoMatrixDisplay.h"

static OpenSkyFetcher   g_openSky;
static AeroAPIFetcher   g_aeroApi;
static LocalLogoStore   g_logoStore;
static FlightDataFetcher *g_fetcher = nullptr;
static NeoMatrixDisplay g_display;

static unsigned long g_lastFetchMs = 0;

void setup()
{
    Serial.begin(115200);
    delay(200);

    g_display.initialize();
    g_display.displayMessage(String("FlightWall"));

    // Mount LittleFS for local logo storage.
    // Failure is non-fatal — logo display is simply skipped.
    g_logoStore.initialize();

    if (strlen(WiFiConfiguration::WIFI_SSID) > 0)
    {
        WiFi.mode(WIFI_STA);
        g_display.displayMessage(String("WiFi: ") + WiFiConfiguration::WIFI_SSID);
        WiFi.begin(WiFiConfiguration::WIFI_SSID, WiFiConfiguration::WIFI_PASSWORD);
        Serial.print("Connecting to WiFi");
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 50)
        {
            delay(200);
            Serial.print(".");
            attempts++;
        }
        Serial.println();
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.print("WiFi connected: ");
            Serial.println(WiFi.localIP());
            g_display.displayMessage(String("WiFi OK ") + WiFi.localIP().toString());
            delay(3000);
            g_display.showLoading();
        }
        else
        {
            Serial.println("WiFi not connected; proceeding without network");
            g_display.displayMessage(String("WiFi FAIL"));
        }
    }

    g_fetcher = new FlightDataFetcher(&g_openSky, &g_aeroApi, &g_logoStore);
}

void loop()
{
    const unsigned long intervalMs = TimingConfiguration::FETCH_INTERVAL_SECONDS * 1000UL;
    const unsigned long now = millis();
    if (now - g_lastFetchMs >= intervalMs)
    {
        g_lastFetchMs = now;

        std::vector<StateVector> states;
        std::vector<FlightInfo> flights;
        size_t enriched = g_fetcher->fetchFlights(states, flights);

        Serial.print("OpenSky state vectors: ");
        Serial.println((int)states.size());
        Serial.print("AeroAPI enriched flights: ");
        Serial.println((int)enriched);

        for (const auto &s : states)
        {
            Serial.print(" ");
            Serial.print(s.callsign);
            Serial.print(" @ ");
            Serial.print(s.distance_km, 1);
            Serial.print("km bearing ");
            Serial.println(s.bearing_deg, 1);
        }

        for (const auto &f : flights)
        {
            Serial.println("=== FLIGHT INFO ===");
            Serial.print("Ident: ");        Serial.println(f.ident);
            Serial.print("Airline: ");      Serial.println(f.airline_display_name_full);
            Serial.print("Aircraft: ");     Serial.println(f.aircraft_display_name_short.length()
                                                           ? f.aircraft_display_name_short
                                                           : f.aircraft_code);
            Serial.printf("Origin: %s (%s) %s\n", f.origin.code_iata.c_str(), f.origin.code_icao.c_str(), f.origin.name.c_str());
            Serial.printf("Destination: %s (%s) %s\n", f.destination.code_iata.c_str(), f.destination.code_icao.c_str(), f.destination.name.c_str());
            Serial.print("Logo pixels: ");  Serial.println((int)f.airline_logo_rgb565.size());
            Serial.println("===================");
        }

        g_display.displayFlights(flights);
    }
    delay(10);
}
