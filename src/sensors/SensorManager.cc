#include "SensorManager.h"
#include "messages/SensorMessages_m.h"
#include "inet/transportlayer/contract/quic/QuicCommand_m.h" // QuicStreamReq

Define_Module(SensorManager);

SensorManager::~SensorManager() {}

void SensorManager::initialize() {
    registerSensors();
}

void SensorManager::registerSensors() {
    int sensorsNumber = gateSize("toSensors");
    sensors.resize(sensorsNumber);

    for (int i = 0; i < sensorsNumber; i++) {
        cModule *sensor = gate("toSensors", i)->getPathEndGate()->getOwnerModule(); // Gets the sensor module from the gate

        sensors[i].streamId = sensor->par("streamID");
        sensors[i].type = sensor->par("sensorType").stdstringValue();

        EV_INFO << "Registered: type = " << sensors[i].type << " streamId = " << sensors[i].streamId << endl;
    }
}

void SensorManager::handleMessage(cMessage *msg) {
    if (msg->arrivedOn("fromApp")) { retrieveData(); delete msg; }
    else if (msg->arrivedOn("fromSensors")) sendDataToApp(msg);
    else delete msg;
}

void SensorManager::retrieveData() {
    frameId++;

    int sensorsNumber = gateSize("toSensors");

    for (int i = 0; i < sensorsNumber; i++) {
        auto pkt = new Packet("SensorReq");
        auto req = makeShared<SensorDataRequest>();

        req->setFrameId(frameId);
        req->setRequestTime(simTime());
        req->setChunkLength(B(8));
        pkt->insertAtBack(req);
        send(pkt, "toSensors", i);
    }
}

void SensorManager::sendDataToApp(cMessage* msg) {
    Packet *pkt = dynamic_cast<Packet*>(msg);
    if (pkt == nullptr) {
        delete msg;
        return;
    }

    int idx = pkt->getArrivalGate()->getIndex();
    int streamId = sensors[idx].streamId;
    pkt->addTagIfAbsent<QuicStreamReq>()->setStreamID(streamId);

    EV_INFO << "Inoltro dati sensore[" << idx << "] su stream " << streamId << endl;
    send(pkt, "toApp");
}
