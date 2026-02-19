/*
Purpose: Orchestrate fetching and enrichment of flight data for display.
Flow:
1) Use BaseStateVectorFetcher to fetch nearby state vectors by geo filter.
2) For each callsign, use BaseFlightFetcher (AeroAPI) to retrieve FlightInfo.
3) Enrich names and airline logo via FlightWallFetcher.
4) Copy ADS-B telemetry (altitude, speed, heading, vertical rate) from the
   paired StateVector into FlightInfo for display on the bottom two lines.
Output: Returns count of enriched flights and fills outStates/outFlights.
*/
#include "core/FlightDataFetcher.h"
#include "config/UserConfiguration.h"
#include "adapters/FlightWallFetcher.h"

FlightDataFetcher::FlightDataFetcher(BaseStateVectorFetcher *stateFetcher,
                                     BaseFlightFetcher      *flightFetcher,
                                     BaseLogoStore          *logoStore)
    : _stateFetcher(stateFetcher),
      _flightFetcher(flightFetcher),
      _logoStore(logoStore) {}

size_t FlightDataFetcher::fetchFlights(std::vector<StateVector> &outStates,
                                       std::vector<FlightInfo>  &outFlights)
{
    outStates.clear();
    outFlights.clear();

    bool ok = _stateFetcher->fetchStateVectors(
        UserConfiguration::CENTER_LAT,
        UserConfiguration::CENTER_LON,
        UserConfiguration::RADIUS_KM,
        outStates);
    if (!ok)
        return 0;

    // Constructed once per cycle to avoid per-flight object construction.
    FlightWallFetcher fw;

    size_t enriched = 0;
    for (const StateVector &s : outStates)
    {
        if (s.callsign.length() == 0)
            continue;

        FlightInfo info;
        if (_flightFetcher->fetchFlightInfo(s.callsign, info))
        {
            // Copy ADS-B telemetry from the paired state vector.
            // These feed the bottom two display lines (alt/speed, track/vr).
            info.sv_baro_altitude = s.baro_altitude;
            info.sv_velocity      = s.velocity;
            info.sv_heading       = s.heading;
            info.sv_vertical_rate = s.vertical_rate;

            // Airline display name
            if (info.operator_icao.length())
            {
                String airlineFull;
                if (fw.getAirlineName(info.operator_icao, airlineFull))
                    info.airline_display_name_full = airlineFull;

                // Logo — local filesystem lookup, silent fallback on miss.
                if (_logoStore)
                    _logoStore->getAirlineLogo(info.operator_icao,
                                               info.airline_logo_rgb565);
            }

            // Aircraft display name
            if (info.aircraft_code.length())
            {
                String aircraftShort, aircraftFull;
                if (fw.getAircraftName(info.aircraft_code, aircraftShort, aircraftFull))
                {
                    if (aircraftShort.length())
                        info.aircraft_display_name_short = aircraftShort;
                }
            }

            outFlights.push_back(info);
            enriched++;
        }
    }
    return enriched;
}
