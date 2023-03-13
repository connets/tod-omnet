package tod_network.carla_omnet;

import carlanet.CarlanetManager;

simple TodCarlanetManager extends CarlanetManager
{
    parameters:
        @class(TodCarlanetManager);
        networkActiveModuleType = "CarlaCar"; 
        string carlaConfiguration;
        
}

