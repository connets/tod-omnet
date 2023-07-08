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
- `carlaConfiguration`: the specific CARLA world configuration. This must be one of the configurations available in the CARLA side components in the [configuration folder](https://github.com/connets/tod-carla/tree/main/configuration/world) 

``` ini
######### CARLA manager configurations #################
*.carlaCommunicationManager.host = "localhost"  
*.carlaCommunicationManager.port = 5555
*.carlaCommunicationManager.simulationTimeStep = 10ms
*.carlaCommunicationManager.carlaConfiguration = "world_4"   # file world_4.yaml is present in CARLA side world configurations folder
```
 
