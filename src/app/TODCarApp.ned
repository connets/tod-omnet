package tod_network.app;

import inet.applications.contract.IApp;

simple TODCarApp like IApp
{
    parameters:
        @class(TODCarApp);
        @display("i=block/app");

        string agentConfiguration = default("normal");
        string destAddress = default("server");
        int destPort = default(3000);
        volatile int statusMessageLength @unit(byte) = default(8kB);

        // Fixed size of an instruction on stream 0 (server->car). MUST match
        // TODAgentApp.instructionMessageLength: framing unit to read one COMPLETE
        // instruction at a time (QUIC may deliver several back-to-back) to avoid
        // chunkSize conversion error.
        int instructionMessageLength @unit(byte) = default(1kB);
        int datagramChunkSize @unit(B) = default(1000B);  // It has to be less then max_datagram_size
        volatile double statusUpdateInterval @unit(s)  = default(0.016s);

        volatile double encodingImageTime @unit(s) = default(10ms);
        volatile double collectionDataTime @unit(s) = default(15ms);

        bool zeroDelay = default(false);

        // --- Qualita' camera adattiva -------------------------------------------
        // Il livello lo DECIDE L'AGENTE e arriva sull'istruzione: da qui si vedono
        // solo le istruzioni che sono tornate, quindi un uplink che scarta la meta'
        // dei frame e' indistinguibile da uno sano. L'auto applica e non discute.
        // Le soglie stanno in TODAgentApp.qualityLossThresholds.
        //
        // Peso del campione nuovo nella EWMA dell'RTT (ora solo statistica).
        double rttEwmaAlpha = default(0.2);
        // Fallback per downlink morto: se non arriva nessuna istruzione non arriva
        // nemmeno la richiesta, quindi l'auto scende di un livello ogni timeout di
        // silenzio. 0 = disattivato.
        double qualitySilenceTimeout @unit(s) = default(0s);
        // Gradino piu' basso a cui il fallback puo' arrivare.
        int qualityMaxLevel = default(3);

        @signal[packetSent](type=inet::Packet);
        @signal[packetReceived](type=inet::Packet);
        @signal[instructionRttEwma];
        @signal[qualityLevel];
        @statistic[instructionRttEwma](title="instruction RTT (EWMA)"; unit=s; source=instructionRttEwma; record=vector,stats; interpolationmode=linear);
        @statistic[qualityLevel](title="camera quality level"; source=qualityLevel; record=vector,stats; interpolationmode=sample-hold);
        @statistic[packetReceived](title="packets received"; source=packetReceived; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[throughput](title="throughput"; unit=bps; source="throughput(packetReceived)"; record=vector);

        @statistic[packetReceivedDelay](title="packets received delay"; source=weightedMeanPerGroup(groupRegionsPerPacket(lifeTimePerRegion(packetReceived))); record=vector; interpolationmode=none);
        @statistic[statusCreationTime](title="status creation time"; source = statusCreationTime(packetSent); record=vector,stats);
        @statistic[instructionRTTNetwork](title="network RTT"; source = instructionRTTNetwork(packetReceived); record=vector,stats);
        @statistic[instructionDelay](title="instruction delay"; source = instructionDelay(packetReceived); record=vector,stats);

    gates:
        input socketIn;
        output socketOut;

        input fromManager; // Data from sensors (handled by sensorManager)
        output toManager;  // Data request from todCarApp to sensors (handled by sensorManager)
}

