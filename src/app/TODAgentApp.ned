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

        // --- Clock proprio dell'agente ------------------------------------------
        // L'agente non apre una finestra quando arrivano i dati: dichiara una
        // cadenza, la avvia al primo datagramma ricevuto da un attore, e da lì
        // decide a istanti fissi su quello che ha rispettato la deadline. Uno slot
        // in cui non arriva nulla e' perdita totale senza casi speciali, per questo
        // non c'e' piu' nessun watchdog.
        //
        // Va tenuta pari alla cadenza di campionamento dell'auto
        // (TODCarApp.statusUpdateInterval), altrimenti i due clock derivano e uno
        // slot vede zero o due frame.
        double samplingInterval @unit(s) = default(0.016667s);

        // Slack di playout: la deadline cade a samplingInterval + deadlineSlack,
        // cosi' un frame a cavallo del confine viene comunque contato. Si paga
        // latenza per comprare loss, e' il compromesso classico del jitter buffer.
        double deadlineSlack @unit(s) = default(0.008s);

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