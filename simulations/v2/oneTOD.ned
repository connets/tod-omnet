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

package tod_network.simulations.v2;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.node.ethernet.Eth10G;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import simu5g.common.binder.Binder;
import simu5g.common.carrierAggregation.CarrierAggregation;
import simu5g.nodes.Upf;
import simu5g.nodes.gNodeB;
import simu5g.nodes.NrUe;
import simu5g.nodes.backgroundCell.BackgroundCell;
import simu5g.world.radio.LteChannelControl;
import tod_network.carla_omnet.TodCarlanetManager;
import tod_network.nodes.CarlaCar;
import tod_network.nodes.EdgeCamera;
import inet.visualizer.common.IntegratedVisualizer;

network oneTOD
{
    parameters:
        int numUe = default(1);
        double bitErrorRate = default(0);
        @display("i=block/network2;bgb=1000,1000;bgi=Town01.jpg");
    submodules:
        visualizer: IntegratedVisualizer {
            @display("p=701.44,168.96");
        }
        channelControl: LteChannelControl {
            @display("p=50,25;is=s");
        }
        routingRecorder: RoutingTableRecorder {
            @display("p=50,75;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=50,125");
        }
        binder: Binder {
            @display("p=50,900;is=s");
        }
        carrierAggregation: CarrierAggregation {
            @display("p=50.993748,258.7;is=s");
        }
        server: StandardHost {
            @display("p=212,118;is=n;i=device/server");
        }
        router: Router {
            @display("p=363.175,115.66875;i=device/smallrouter");
        }
        upf: Upf {
            @display("p=527.35,116.9125");
        }
        gnb: gNodeB {
            @display("p=100,100;is=vl");
        }

        VectorVehicle[0]: CarlaCar {
            @display("p=0,0");
        }

        VectorPedestrian[0]: NrUe {
            @display("p=0,100");
        }

        VectorEdgeCamera[0]: EdgeCamera {
            @display("p=0,200");
        }

        car2: NrUe {
            @display("p=1,1");
        }
        carlaCommunicationManager: TodCarlanetManager;
    connections:
        server.pppg++ <--> Eth10G {  ber = parent.bitErrorRate; } <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> upf.filterGate;
        upf.pppg++ <--> Eth10G <--> gnb.ppp;
}
