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
        // L'auto misura quanto e' vecchia l'istruzione appena arrivata (istante di
        // arrivo meno istante di campionamento del frame che l'ha prodotta), la
        // media con una EWMA e da quella sceglie il livello di qualita' della
        // camera, che i sensori usano per ridurre i byte immessi in rete.
        //
        // Soglie di DISCESA, in ordine crescente: con N soglie i livelli sono N+1
        // e devono corrispondere ai gradini di CameraSensorApp.qualityResolutions.
        // Default vuoto = adattamento DISATTIVATO (camera sempre a piena qualita'),
        // cosi' gli scenari gia' esistenti non cambiano comportamento. Si abilita
        // per scenario, p.es. "60ms 120ms 220ms" in sensors.ini.
        string qualityRttThresholds = default("");
        // Peso del campione nuovo nella EWMA: piu' alto = piu' reattivo, piu' rumoroso.
        double rttEwmaAlpha = default(0.2);
        // Per risalire di un livello l'RTT deve scendere sotto la soglia che aveva
        // fatto scendere, ridotta di questa frazione. Evita il flapping a cavallo
        // della soglia.
        double qualityHysteresis = default(0.2);

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

