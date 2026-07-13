//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

package tod_network.sensors;

import inet.applications.contract.IApp;
import tod_network.app.TODCarApp;
import tod_network.sensors.SensorBaseApp;
import tod_network.sensors.SensorManager;

module SensorsManager like IApp
{
    parameters:
        int sensorNumber = default(0);
        @display("i=block/source;bgb=511,320");

    gates:
        input socketIn;
        output socketOut;

    submodules:
        sensorManager: SensorManager {          // Build above todCarApp: establish connection with sensors
            @display("p=172,80");
        }
        todCarApp: TODCarApp {                  // App that has Quic Socket
            @display("p=77,156");
        }
        sensors[sensorNumber]: <> like SensorBaseApp { // Sensors' array
            @display("p=284,26");
        }
    connections:
        // application <--> network
        todCarApp.socketOut --> socketOut;
        todCarApp.socketIn <-- socketIn;

        // sensorManager <--> todCarApp
        sensorManager.toApp --> todCarApp.fromManager;
        sensorManager.fromApp <-- todCarApp.toManager;

        // sensorManager <--> sensors
        for i=0..sensorNumber-1 {
            sensors[i].fromManager <-- sensorManager.toSensors++;
            sensors[i].toManager --> sensorManager.fromSensors++;
        }
}
