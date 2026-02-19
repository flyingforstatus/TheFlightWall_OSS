#pragma once

#include <Arduino.h>
#include <vector>
#include "AirportInfo.h"

struct FlightInfo
{
    // Flight identifiers
    String ident;
    String ident_icao;
    String ident_iata;

    // Operator
    String operator_code;
    String operator_icao;
    String operator_iata;

    // Route
    AirportInfo origin;
    AirportInfo destination;

    // Aircraft
    String aircraft_code;

    // Human-friendly display strings
    String airline_display_name_full;
    String aircraft_display_name_short;

    // Telemetry from the paired StateVector (set by FlightDataFetcher).
    // Used to render the bottom two lines: altitude/speed and track/vertical-rate.
    double sv_baro_altitude  = NAN;  // metres
    double sv_velocity       = NAN;  // m/s
    double sv_heading        = NAN;  // degrees true
    double sv_vertical_rate  = NAN;  // m/s (positive = climbing)

    // Airline logo: pre-converted RGB565 pixels in row-major order.
    // Dimensions are fixed at AirlineLogo::WIDTH x AirlineLogo::HEIGHT.
    // Empty vector means no logo was fetched or available.
    std::vector<uint16_t> airline_logo_rgb565;
};

// Logo dimensions used throughout the firmware.
// 32x32 fits cleanly in the 34px logo column on both 128x64 and 64x64 panels.
namespace AirlineLogo
{
    static constexpr uint8_t WIDTH  = 32;
    static constexpr uint8_t HEIGHT = 32;
}
