package tod_network.app;

import inet.applications.contract.IApp;

simple TODAgentApp like IApp
{
    parameters:
        @class(TODAgentApp);
        string agentId = default("");
        // Fixed size of the instruction sent back to the car (fits one datagram).
        volatile int instructionMessageLength @unit(byte) = default(30B);

        // Legacy/unused: stream 0 has been removed. Kept so older .ini files that
        // still assign it keep loading.
        int statusMessageLength @unit(byte) = default(1kB);

        string localAddress = default("server");
        int localPort = 3000;

        volatile double processingStatusTime @unit(s) = default(15ms); //time to decode one video frame

        // Reuse of past sensor data when computing the loss ratio. For each
        // expected sensor the reusable fraction is reuseDecay^(missed frames) *
        // (last well-received fraction). 0 disables reuse; higher values keep
        // stale data usable longer.
        double reuseDecay = default(0.5);

        // Expected car sampling cadence, used by the 100%-loss watchdog. Mirror
        // of TODCarApp.statusUpdateInterval (60 fps default). A slot is declared
        // fully lost when no datagram arrives for more than 2*frameInterval.
        double frameInterval @unit(s) = default(0.016667s);

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