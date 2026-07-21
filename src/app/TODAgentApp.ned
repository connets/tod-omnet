package tod_network.app;

import inet.applications.contract.IApp;

simple TODAgentApp like IApp
{
    parameters:
        @class(TODAgentApp);
        string agentId = default("");
        volatile int instructionMessageLength @unit(byte) = default(30B);

        // Fixed size of a status message on stream 0. MUST match
        // TODCarApp.statusMessageLength: it is the framing unit the server uses to
        // read one COMPLETE status at a time (QUIC may split it across packets).
        int statusMessageLength @unit(byte) = default(1kB);

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