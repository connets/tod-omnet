package tod_network.app;

import inet.applications.contract.IApp;


simple TODCarApp like IApp
{
    parameters:
        @class(TODCarApp);
        @display("i=block/app");
        
        string agentConfiguration = default("normal");  	//this must be a type supported by Carla client
        string destAddress = default("server");
        int destPort = 3000;
        volatile int statusMessageLength @unit(byte) = default(8kB);
        double statusUpdateInterval @unit(s) = default(50ms);
        
        @signal[packetSent](type=inet::Packet);
        @signal[packetReceived](type=inet::Packet);
        @statistic[packetReceived](title="packets received"; source=packetReceived; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[throughput](title="throughput"; unit=bps; source="throughput(packetReceived)"; record=vector);
        @statistic[packetReceivedDelay](title="packets received delay"; source=weightedMeanPerGroup(groupRegionsPerPacket(lifeTimePerRegion(packetReceived))); record=vector; interpolationmode=none);
        @statistic[instructionDelay](title="instruction delay"; source =instructionDelay(packetReceived); record=vector,stats);
        
    gates:
        input socketIn;
        output socketOut;
}

