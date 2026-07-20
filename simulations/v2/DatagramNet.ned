package tod_network.simulations.v2;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.node.ethernet.Eth10G;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import simu5g.common.binder.Binder;
import simu5g.common.carrierAggregation.CarrierAggregation;
import simu5g.nodes.gNodeB;
import simu5g.nodes.Upf;
import simu5g.world.radio.LteChannelControl;
import tod_network.carla_omnet.TodCarlanetManager;
import tod_network.nodes.CarlaCar;

//
// Minimal TOD-over-5G co-simulation network for the QUIC DATAGRAM scenario.
//
// Built on simu5g's SingleCell_Standalone layout (which is proven to carry QUIC
// from a UE to the server) PLUS the CARLA bridge. The hero vehicle is a STATIC
// CarlaCar (VectorHero[0]) that registers with the manager (carla_id set in the
// ini) and is therefore spawned in CARLA and driven by it. VectorVehicle[] is a
// size-0 placeholder that the manager grows if CARLA reports extra actors.
//
// IMPORTANT: the UE uses the MAIN Ipv4NetworkConfigurator (NOT HostAutoConfigurator)
// and servingNodeId/nrServingNodeId for cell association — this is what gives the
// UE a working route to the server (see quic_standalone.ini experiments).
//
network DatagramNet
{
    parameters:
        int numUe = default(0);
    submodules:
        channelControl: LteChannelControl { @display("p=50,25;is=s"); }
        routingRecorder: RoutingTableRecorder { @display("p=50,75;is=s"); }
        configurator: Ipv4NetworkConfigurator { @display("p=50,125"); }
        binder: Binder { @display("p=50,175;is=s"); }
        carrierAggregation: CarrierAggregation { @display("p=50,258;is=s"); }
        server: StandardHost { @display("p=212,118;is=n;i=device/server"); }
        router: Router { @display("p=363,115;i=device/smallrouter"); }
        upf: Upf { @display("p=527,116"); }
        iUpf: Upf { @display("p=725,118"); }
        gnb: gNodeB { @display("p=726,277;is=vl"); }
        VectorHero[1]: CarlaCar { @display("p=628,411"); }          // static TOD ego
        VectorVehicle[0]: CarlaCar { @display("p=628,470"); }       // placeholder for CARLA-spawned actors
        carlaCommunicationManager: TodCarlanetManager { @display("p=50,340;is=s"); }
    connections:
        server.pppg++ <--> Eth10G <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> upf.filterGate;
        upf.pppg++ <--> Eth10G <--> iUpf.pppg++;
        iUpf.pppg++ <--> Eth10G <--> gnb.ppp;
}
