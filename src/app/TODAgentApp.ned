package tod_network.app;

import inet.applications.contract.IApp;



simple TODAgentApp like IApp
{
    parameters:
        @class(TODAgentApp);
        string agentId;
        volatile int instructionMessageLength @unit(byte) = default(30B);
        
        string localAddress = default("server");
        int localPort = 3000;
        
        volatile double processingStatusTime @unit(s) = default(15ms); //time to decode one video frame
        @signal[packetSent](type=inet::Packet);
        @signal[packetReceived](type=inet::Packet);
        @statistic[packetReceived](title="packets received"; source=packetReceived; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[throughput](title="throughput"; unit=bps; source="throughput(packetReceived)"; record=vector);
        @statistic[packetReceivedDelay](title="packets received delay"; source=weightedMeanPerGroup(groupRegionsPerPacket(lifeTimePerRegion(packetReceived))); record=vector; interpolationmode=none);
		@statistic[processStatusTime](title="process status time"; source = processStatusTime(packetSent); record=vector,stats);
        
    gates:
        input socketIn;
        output socketOut;

}

