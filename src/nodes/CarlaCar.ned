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

package tod_network.nodes;

import simu5g.nodes.NrUe;
import inet.transportlayer.contract.IQuic;

module CarlaCar extends NrUe
{
    parameters:
        mobility.typename = default("TodCarlaInetMobility");

        *.routingTableModule = default(absPath(".ipv4.routingTable"));

        @display("i=misc/car,,0;is=n");

    submodules:
        quic: <default(firstAvailableOrEmpty("Quic"))> like IQuic {
        	@display("p=825,226");
        }

    connections:
        // connections with ip layer
        quic.udpIn <-- tn.out++;
        quic.udpOut --> tn.in++;

        // connections with app layer
		quic.appIn <-- at.out++;
		quic.appOut --> at.in++;

}
