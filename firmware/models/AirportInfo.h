#pragma once

#include <Arduino.h>

struct AirportInfo
{
    String code_icao;
    String code_iata;
    String name;  // Full airport name e.g. "San Francisco Intl"
};
