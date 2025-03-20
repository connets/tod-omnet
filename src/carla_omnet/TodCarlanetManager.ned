package tod_network.carla_omnet;

import carlanet.CarlanetManager;

simple TodCarlanetManager extends CarlanetManager
{
    parameters:
        @class(TodCarlanetManager);
        string carlaConfiguration = default("");
}

