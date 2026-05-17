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

network General
{
    parameters:
        int numUe = default(1);
        double bitErrorRate = default(0);
        int numUeD2DTx = default(0);
        int numTodBgUes = default(0);
        @display("i=block/network2;p=1000,2000;bgb=2000,1000");
    submodules:
        channelControl: LteChannelControl {
            @display("p=900,100;is=s");
        }
        routingRecorder: RoutingTableRecorder {
            @display("p=900,250;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=900,400");
        }
        binder: Binder {
            @display("p=900,550;is=s");
        }
        carrierAggregation: CarrierAggregation {
            @display("p=900,700;is=s");
        }
        server: StandardHost {
            @display("p=569,196;is=n;i=device/server");
        }
        router: Router {
            @display("p=569,99;i=device/smallrouter");
        }
        upf: Upf {
            @display("p=482,100");
        }
        gnb: gNodeB {
            @display("p=-540,400");
        }
        gnb2: gNodeB {
            @display("p=-540,-450");
        }
        gnb3: gNodeB {
            @display("p=420,-450");
        }
        gnb4: gNodeB {
            @display("p=420,400");
        }

        VectorVehicle[0]: CarlaCar {
            @display("p=150,500");
        }

        VectorPedestrian[0]: NrUe {
            @display("p=150,650");
        }

        VectorEdgeCamera[0]: EdgeCamera {
            @display("p=150,800");
        }

        VectorNRUeDISTURB[0]: NrUe {
            @display("p=350,650");
        }

        ueD2DTx[numUeD2DTx]: NrUe {
            @display("p=550,400");
        }

        bgUeTod_1[numTodBgUes]: NrUe {
            @display("p=550,500");
            @display("i=misc/car2");
        }


        carlaCommunicationManager: TodCarlanetManager {
            @display("p=900,850;is=s");
        }
        //NECESSARY TO CORRECT ROUTE PACKET (AGENT->VEHICLE) IN ROUTER
        visualizer: IntegratedVisualizer {
            @display("p=701.44,168.96");
        }
    connections:
        server.pppg++ <--> Eth10G {  ber = parent.bitErrorRate; } <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> upf.filterGate;
        upf.pppg++ <--> Eth10G <--> gnb.ppp;
        upf.pppg++ <--> Eth10G <--> gnb2.ppp;
        upf.pppg++ <--> Eth10G <--> gnb3.ppp;
        upf.pppg++ <--> Eth10G <--> gnb4.ppp;
        //# X2 connections
        gnb.x2++ <--> Eth10G <--> gnb2.x2++;
        gnb.x2++ <--> Eth10G <--> gnb3.x2++;
        gnb.x2++ <--> Eth10G <--> gnb4.x2++;
        gnb2.x2++ <--> Eth10G <--> gnb3.x2++;
        gnb2.x2++ <--> Eth10G <--> gnb4.x2++;
        gnb3.x2++ <--> Eth10G <--> gnb4.x2++;
}
