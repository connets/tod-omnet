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
import tod_network.nodes.CarlaCar;

//
// Standalone QUIC-over-5G test (NO CARLA): a CarlaCar UE runs a QUIC datagram
// sender toward a StandardHost server over a single NR cell. Used to debug the
// QUIC-on-simu5g-UE transport in isolation (reproduces the vehicle's QUIC path
// without the CarlanetManager blocking init).
//
network QuicUeTest
{
    parameters:
        int numUe = default(1);
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
        ue[numUe]: CarlaCar { @display("p=628,411"); }
    connections:
        server.pppg++ <--> Eth10G <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> upf.filterGate;
        upf.pppg++ <--> Eth10G <--> iUpf.pppg++;
        iUpf.pppg++ <--> Eth10G <--> gnb.ppp;
}
