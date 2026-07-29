package tod_network.simulations.v2;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.node.ethernet.Eth10G;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import simu5g.common.binder.Binder;
import simu5g.common.carrierAggregation.CarrierAggregation;
import simu5g.nodes.gNodeB;
import simu5g.nodes.NrUe;
import simu5g.nodes.Upf;
import simu5g.world.radio.LteChannelControl;
import tod_network.carla_omnet.TodCarlanetManager;
import tod_network.nodes.CarlaCar;

//
// Minimal TOD-over-5G co-simulation network for the QUIC DATAGRAM scenario.
//
// Built on simu5g's SingleCell_Standalone layout (which is proven to carry QUIC
// from a UE to the server) PLUS the CARLA bridge.
//
// ALL vehicles come from CARLA. The manager grows VectorVehicle[] as CARLA reports
// the actors declared in the tod-carla config yaml, in yaml order: the ego is the
// first entry, so it is VectorVehicle[0], and the ini configures it there.
//
// There used to be a STATIC ego here (VectorHero[0]) that registered with the
// manager through mobility.carla_id and was announced to CARLA in moving_actors.
// That pattern is gone: in the current tod-carla
// ActorManager.create_actors_from_omnet has an empty body, so actors announced by
// OMNeT are silently ignored - CARLA never spawns them, never reports them back,
// and at the first simulation step the manager treats them as destroyed and
// deletes the module together with all its apps.
//
// IMPORTANT: the UE uses the MAIN Ipv4NetworkConfigurator (NOT HostAutoConfigurator)
// and servingNodeId/nrServingNodeId for cell association — this is what gives the
// UE a working route to the server (see quic_standalone.ini experiments).
//
// FOUR-CELL deployment: the ego drives a ~1455 m loop (city_trip_world_4), so a
// single cell leaves it up to 341 m away from the antenna for a good part of the
// route. Four gNBs placed along the path cut the worst-case distance to ~127 m.
// The cells are fully meshed over X2 because any of them can be the source or the
// target of a handover; the handover itself is enabled in the ini.
//
network DatagramNet
{
    parameters:
        int numUe = default(0);
        // Non-TOD road traffic: real CARLA vehicles (spawned and driven by tod-carla
        // as 'passive' actors along their own route) that are ALSO 5G UEs, so they
        // are visible in the CARLA world AND they load the cells.
        int numTrafficVehicles = default(0);
        // Synthetic radio-only load: plain NR UEs with no CARLA actor behind them.
        // Useful to push the offered load past what the CARLA config files cover.
        int numBgVehicles = default(0);
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
        // Layout roughly mirrors the real placement set in the ini (NW -> SE).
        gnb1: gNodeB { @display("p=560,255;is=vl"); }
        gnb2: gNodeB { @display("p=670,290;is=vl"); }
        gnb3: gNodeB { @display("p=780,325;is=vl"); }
        gnb4: gNodeB { @display("p=890,360;is=vl"); }
        VectorVehicle[0]: CarlaCar { @display("p=628,519"); }       // grown by the manager: [0] is the ego
        VectorTraffic[numTrafficVehicles]: CarlaCar { @display("p=628,578;i=misc/car2"); }  // CARLA road traffic, also UEs
        bgVehicle[numBgVehicles]: NrUe { @display("p=628,637;i=misc/car2"); }   // synthetic radio load only
        carlaCommunicationManager: TodCarlanetManager { @display("p=50,340;is=s"); }
    connections:
        server.pppg++ <--> Eth10G <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> upf.filterGate;

        //# 5G core: one intermediate UPF fans out to the four cells
        upf.pppg++ <--> Eth10G <--> iUpf.pppg++;
        iUpf.pppg++ <--> Eth10G <--> gnb1.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb2.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb3.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb4.ppp;

        //# X2 full mesh. The comment on each line is the x2ppp interface index the
        //# link gets on each side (gates are numbered in connection order): the ini
        //# needs it to write the x2App connectAddress of every peer.
        gnb1.x2++ <--> Eth10G <--> gnb2.x2++;   // gnb1:x2ppp0 <-> gnb2:x2ppp0
        gnb1.x2++ <--> Eth10G <--> gnb3.x2++;   // gnb1:x2ppp1 <-> gnb3:x2ppp0
        gnb1.x2++ <--> Eth10G <--> gnb4.x2++;   // gnb1:x2ppp2 <-> gnb4:x2ppp0
        gnb2.x2++ <--> Eth10G <--> gnb3.x2++;   // gnb2:x2ppp1 <-> gnb3:x2ppp1
        gnb2.x2++ <--> Eth10G <--> gnb4.x2++;   // gnb2:x2ppp2 <-> gnb4:x2ppp1
        gnb3.x2++ <--> Eth10G <--> gnb4.x2++;   // gnb3:x2ppp2 <-> gnb4:x2ppp2
}
