#pragma once

#include <Arduino.h>
#include <vector>
#include "interfaces/BaseStateVectorFetcher.h"
#include "interfaces/BaseFlightFetcher.h"
#include "interfaces/BaseLogoStore.h"
#include "models/StateVector.h"
#include "models/FlightInfo.h"

class FlightDataFetcher
{
public:
    // logoStore may be nullptr — logo lookup is simply skipped in that case.
    FlightDataFetcher(BaseStateVectorFetcher *stateFetcher,
                      BaseFlightFetcher     *flightFetcher,
                      BaseLogoStore         *logoStore = nullptr);

    size_t fetchFlights(std::vector<StateVector> &outStates,
                        std::vector<FlightInfo>  &outFlights);

private:
    BaseStateVectorFetcher *_stateFetcher;
    BaseFlightFetcher      *_flightFetcher;
    BaseLogoStore          *_logoStore;
};
