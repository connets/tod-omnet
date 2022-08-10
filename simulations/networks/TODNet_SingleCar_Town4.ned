// 
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
// 
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself, 
// and cannot be removed from it.
// 
package tod_network.simulations.networks;
//import org.carla_omnet.CarlaCommunicationManager;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.node.ethernet.Eth10G;
import inet.node.ethernet.Eth100M;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import simu5g.common.binder.Binder;
import simu5g.common.carrierAggregation.CarrierAggregation;
import simu5g.nodes.Upf;
import simu5g.nodes.NR.gNodeB;
import simu5g.nodes.NR.NRUe;
import simu5g.nodes.backgroundCell.BackgroundCell;
import simu5g.world.radio.LteChannelControl;
import tod_network.carla_omnet.CarlaCommunicationManager;
import tod_network.nodes.CarlaCar;

// SingleCell_standalone
//
// This network represents the deployment option 3 in 3GPP 38.801
//
network TODNetTown4
{
    parameters:
        int numUe = default(1);
        bool withBgBackhaul = default(false);
        double bitErrorRate = default(0);
        @display("i=block/network2;p=-600,-500;b=100,100,rect");
    submodules:
        channelControl: LteChannelControl {
            @display("p=568.264,857.41595;is=s");
        }
        routingRecorder: RoutingTableRecorder {
            @display("p=301.19998,839.344;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=136.54399,823.27997");
        }
        binder: Binder {
            @display("p=120.479996,931.712;is=s");
        }
        carrierAggregation: CarrierAggregation {
            @display("p=210.84,931.712;is=s");
        }
        server: StandardHost {
            @display("p=1008.016,116.464;is=n;i=device/server");
        }
        router: Router {
            @display("p=771.07196,114.45599;i=device/smallrouter");
        }
        upf: Upf {
            @display("p=527.35,116.9125");
        }
        gnb1: gNodeB {
            //@display("p=210.84,435.736;is=vl");
            @display("p=-540,400");
        }
        gnb2: gNodeB {
            //@display("p=210.84,435.736;is=vl");
            @display("p=-540,-450");
        }
        gnb3: gNodeB {
            //@display("p=210.84,435.736;is=vl");
            @display("p=420,-450");
        }
        gnb4: gNodeB {
            //@display("p=210.84,435.736;is=vl");
            @display("p=420,400");
        }
        car: CarlaCar {
            @display("p=13,-318");
        }
        carlaCommunicationManager: CarlaCommunicationManager {
            @display("p=801.19196,901.592");
        }

        bgBhClient: StandardHost if withBgBackhaul {
            @display("p=771,291");
        }

    connections:
        server.pppg++ <--> Eth100M {  ber = parent.bitErrorRate; } <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> upf.filterGate;
        bgBhClient.ethg++ <--> Eth10G <--> router.ethg++;

        upf.pppg++ <--> Eth10G <--> gnb1.ppp;
        upf.pppg++ <--> Eth10G <--> gnb2.ppp;
        upf.pppg++ <--> Eth10G <--> gnb3.ppp;
        upf.pppg++ <--> Eth10G <--> gnb4.ppp;
		//# X2 connections
        gnb1.x2++ <--> Eth10G <--> gnb2.x2++;
        gnb1.x2++ <--> Eth10G <--> gnb3.x2++;
        gnb1.x2++ <--> Eth10G <--> gnb4.x2++;
        gnb2.x2++ <--> Eth10G <--> gnb3.x2++;
        gnb2.x2++ <--> Eth10G <--> gnb4.x2++;
        gnb3.x2++ <--> Eth10G <--> gnb4.x2++;


}