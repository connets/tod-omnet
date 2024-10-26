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

//
// TODO auto-generated module
//
simple CooperativePerceptionCarApp like IApp
{
    parameters:
        @class(CooperativePerceptionCarApp);

        string destAddressesCoop = default(""); // list of IP addresses, separated by spaces ("": don't send)
        int destPort = 3000;
        volatile int statusMessageLengthCoop @unit(byte) = default(1kB);
        //volatile int frameRate = default(60);
        volatile double statusUpdateIntervalCoop @unit(s)  = default(0.016s);

        volatile double encodingImageTime @unit(s) = default(10ms); //time to encode image
        volatile double collectionDataTime @unit(s) = default(15ms); //time to collect sensors data
    
    gates:
        input socketIn;
        output socketOut;
}