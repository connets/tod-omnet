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

package tod_network.carla_omnet.hostconfigurator;

import inet.networklayer.configurator.ipv4.Ipv4NodeConfigurator;

simple Ipv4NodeConfiguratorExtended extends Ipv4NodeConfigurator
{
    parameters:
        string interfaces = default("wlan");         // List of interfaces to autoassign addresses to, separated by a single space character
        string addressBase = default("10.0.0.0");    // Start of the address range from which to automatically assign an address to the autoassignInterfaces
        string netmask = default("255.0.0.0");       // Subnet netmask in which to automatically assign an address to the autoassignInterfaces
        

	@class(Ipv4NodeConfiguratorExtended);
}
