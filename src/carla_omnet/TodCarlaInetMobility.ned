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

package tod_network.carla_omnet;

import carlanet.CarlaInetMobility;
//
// TODO auto-generated module
//
simple TodCarlaInetMobility extends CarlaInetMobility {
	parameters:
   		@class(TodCarlaInetMobility);  
        //Specify the name of the route, <empty string> means that the route is randomly defined by Carla
        string route = default("");
        string agent_configuration;
        string configuration_id; 
}
