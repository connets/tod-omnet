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
        
        volatile double Tc @unit(s) = default(8.33ms); //time to capture one video frame
        volatile double Te @unit(s) = default(3.33ms); //time to encode one video frame
        volatile double Txl @unit(s) = default(10ms); //car command exec delay
        
        
        
        @signal[packetSent](type=inet::Packet);
        @signal[packetReceived](type=inet::Packet);
        @statistic[packetReceived](title="packets received"; source=packetReceived; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[throughput](title="throughput"; unit=bps; source="throughput(packetReceived)"; record=vector);
        
        @statistic[packetReceivedDelay](title="packets received delay"; source=weightedMeanPerGroup(groupRegionsPerPacket(lifeTimePerRegion(packetReceived))); record=vector; interpolationmode=none);
        @statistic[RetrievalStatus](title="retrieval status delay"; source = InstructionDelay(packetSent); record=vector,stats);
        @statistic[instructionRTTNetwork](title="network RTT"; source = instructionRTTNetwork(packetReceived); record=vector,stats);
        @statistic[instructionDelay](title="instruction delay"; source = RetrieveStatus(packetReceived); record=vector,stats);
        
    gates:
        input socketIn;
        output socketOut;
}

