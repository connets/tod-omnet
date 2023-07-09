# Tele-operated Driving | OMNeT++

This software component is the OMNeT++ side of tele-operated driving simulator based on [OMNeT++](https://omnetpp.org/) and [CARLA](https://carla.org/)


## Disclaimer

This software component is an open source simulator licensed under MIT Licence, and based on 
OMNeT framework which is available under the Academic Public License and 
a commercial license (see https://omnest.com/licensingfaq.php). You are 
solely responsible for obtaining the appropriate license for your use(s) 
of OMNeT.
Neither the Università degli Studi di Milano, nor the authors of this software, are 
responsible for obtaining any such licenses, nor liable for any licensing
fees due in connection with your use of OMNeT.


This software is provided on an "as is" basis, without warranties of
any kind, either express or implied, including, but not limited to, 
warranties of accuracy, adequacy, validity, reliability or compliance 
for any specific purpose. Neither the Università degli Studi di Milano, nor the 
authors of this software, are liable for any loss, expense or damage 
of any type that may arise in using this software. 

If you use this software or part of it for your research, please cite 
our work:
  
V. Cislaghi, C. Quadri, V. Mancuso and M. A. Marsan, "Simulation of Tele-Operated Driving over 5G Using CARLA and OMNeT++," 2023 IEEE Vehicular Networking Conference (VNC), Istanbul, Turkiye, 2023, pp. 81-88, doi: [10.1109/VNC57357.2023.10136340](https://doi.org/10.1109/VNC57357.2023.10136340).

If you include this software or part of it within your own software, 
README and LICENSE files cannot be removed from it and must be included 
in the root directory of your software package.


## Dependencies

This version requires:
- OMNeT++ 6.0+
- INET 4.4+
- [Simu5G 1.2.2](http://simu5g.org/)
- [CARLANeTpp](https://github.com/carlanet/carlanetpp/)

**Note:** Docker installation manages all the dependencies. If you need to change some of the dependencies, carefully modify the [`Dockerfile`](Dockerfile) 


## Setup
Please

## Configuration
In the following the `.ini` file configuration options for different aspects of the  Tele-operated Driving simulator
### CarlaNet connection configuration
The connection between the OMNeT++ side and the CARLA side component ([tod-carla](https://github.com/connets/tod-carla)) is realized through a singleton module of type `TodCarlanetManager`, which must be included in the network file (in this example `carlaCommunicationManager` is the name of the module).

Connection parameters:
- `host`: IP address/hostname of the host where tod-carla component is running. 
- `port`: listening port of tod-carla component

**Note:** If you run the simulator using `docker compose` command, the host must be the container name as specified in the `docker-compose.yaml`

CARLA world configuration parameters:
- `simulationTimeStep`: length of the fixed time step of *CARLA synchronous mode* 
- `carlaConfiguration`: the specific CARLA world configuration. This must be one of the world configurations available in the CARLA side component in the [world configuration folder](https://github.com/connets/tod-carla/tree/main/configuration/world) 

``` ini
######### CARLA manager configurations #################
*.carlaCommunicationManager.host = "localhost"  
*.carlaCommunicationManager.port = 5555
*.carlaCommunicationManager.simulationTimeStep = 10ms
*.carlaCommunicationManager.carlaConfiguration = "world_4"   # file world_4.yaml is present in CARLA side world configurations folder
```
 
### Vehicle Mobility and Remote Agent configuration
Every vehicle that needs to be driven by a remote driver must include a mobility model of type [`TodCarlaInetMobility`](src/carla_omnet/TodCarlaInetMobility.ned), which extends `CarlaInetMobility` of [CARLANeTpp](https://github.com/carlanet/carlanetpp/).

Vehicle type and mobility parameters:
- `carlaActorType`: this parameter is inherited from `CarlaInetMobility` and tells CARLA side which CARLA actor to instantiate for this OMNeT++ module. The default value is `"car"`
- `route` (optional): the route name of the specific route that the vehicle has to follow. If it is specified, the route name must be one of the routes available in the [route configuration folder](https://github.com/connets/tod-carla/tree/main/configuration/route) in the CARLA side component. The default value is an empty string `""` which means that CARLA randomly defines the vehicle route at generation time.
- `configuration_id`: the configuration of the CARLA actor associated with the OMNeT++ module. The value must be one of the actor configurations available in the [actor configuration folder](https://github.com/connets/tod-carla/tree/main/configuration/actor) in the CARLA side component.
- `agent_configuration`: the configuration of the CARLA agent controlling the CARLA actor. The value must be one of the agent configurations available in the [agent configuration folder](https://github.com/connets/tod-carla/tree/main/configuration/agent) in the CARLA side component.

Example (`car` module is of type [`CarlaCar`](src/nodes/CarlaCar.ned)):
``` ini
**.car.mobility.route = "city_trip_world_4"  # file city_trip_world_4.yaml is present in CARLA side route configurations folder
**.car.mobility.configuration_id = "simple_vehicle"  #simple_vehicle.yaml is present in CARLA side actor configurations folder  
**.car.mobility.agent_configuration = "simple_agent"  #simple_agent.yaml" is present in CARLA side agent configurations folder
```
### ToD service configuration
Tele-operated Driving service is composed of two applications that use UDP protocol as transport layer and CARLANeT APIs through [CARLANeTpp](https://github.com/carlanet/carlanetpp/) framework. 

#### Car on-board application
[`TODCarApp`](src/app/TODCarApp.ned) simulates the application on-board the vehicle and is responsible for uploading the vehicle status to the remote server and actuating the remote instructions (using CARLANeT APIs).
It is a UDP-based client application and the following parameters can be configured:

*Networking parameters:*
- `destAddress`: address/hostname of the remote server where the remote driver application is deployed
- `destPort`: port number on which the remote driver application is listening

*Application parameters:*
- `statusMessageLength`: size in bytes of the status message, i.e., the message containing all the data of the on-board sensors and video frame.  
- `frameRate`: how many status updates per second.
- `encodingImageTime`: simulates the time for encoding the video frame data 
- `collectionDataTime`: simulates the time for collection of the sensors (e.g., accounting for sensor/video time, CAN bus, etc...) 

#### Remote driver application
[`TODAgentApp`](src/app/TODAgentApp.ned) simulates the console application of the remote driver and is responsible for receiving the vehicle status and computing the instructions (using CARLANeT APIs).
It is a UPD-based server application and the following parameters can be configured:

*Networking parameters:*
- `localAddress`: address/hostname of the host on which the application is deployed
- `localPort`: listening port number 

*Application parameters*:
- `agentId`: the ID of the agent controlling the vehicle on the CARLA side component
- `instructionMessageLength`: size in bytes of the instruction
- `processingStatusTime`: simulates the time for processing the received status message
