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

package tod_network.app;

import inet.applications.contract.IApp;

simple CooperativePerceptionCarApp like IApp
{
    parameters:
        @class(CooperativePerceptionCarApp);
        
        string interfaceTableModule = default("^.interfaceTable");   // The path to the InterfaceTable module
        string multicastGroup = default(""); // if non-empty, makes the socket receive multicast packets sent to that group

        string destAddressesCoop = default("224.0.0.3"); // list of IP addresses, separated by spaces ("": don't send)
        int destPort = 5000;
        volatile int statusMessageLengthCoop @unit(byte) = default(1kB); // TODO choose based on number of vehicles
        volatile double statusUpdateIntervalCoop @unit(s)  = default(0.016s);

        volatile double encodingImageTime @unit(s) = default(10ms); //time to encode image
        volatile double collectionDataTime @unit(s) = default(15ms); //time to collect sensors data
        
        string agentId = default("");
        bool onlyReceive = default(false);
        
        //string interfaces = default("cellular lo0");         // List of interfaces to autoassign addresses to, separated by a single space character
        //string addressBase = default("10.0.0.0");    // Start of the address range from which to automatically assign an address to the autoassignInterfaces
        //string netmask = default("255.0.0.0");       // Subnet netmask in which to automatically assign an address to the autoassignInterfaces
    
    gates:
        input socketIn;
        output socketOut;
}