#include "SensorManager.h"
#include "messages/SensorMessages_m.h"
#include "inet/transportlayer/contract/quic/QuicCommand_m.h"

Define_Module(SensorManager);

SensorManager::~SensorManager() {}

void SensorManager::initialize()
{
    registerSensors();
}

void SensorManager::registerSensors()
{
    int sensorsNumber = gateSize("toSensors");
    sensors.resize(sensorsNumber);

    for (int i = 0; i < sensorsNumber; i++)
    {
        /*
         * Gets the sensor module from the gate
         */
        cModule *sensor = gate("toSensors", i)->getPathEndGate()->getOwnerModule();

        sensors[i].streamId = sensor->par("streamID");
        sensors[i].type = sensor->par("sensorType").stdstringValue();

        EV_INFO << "Registered: type = " << sensors[i].type << " streamId = " << sensors[i].streamId << endl;
    }
}

void SensorManager::handleMessage(cMessage *msg)
{
    if (msg->arrivedOn("fromApp"))
    {
        /*
         * The message arrived from the Car: its time to get the data
         * from the sensors. The poll carries the quality level the car decided
         * from the measured instruction RTT; an ordinary cMessage (older callers)
         * means "stay at the best level".
         */
        int qualityLevel = 0;
        if (auto collect = dynamic_cast<SensorCollectRequest*>(msg))
        {
            qualityLevel = collect->getQualityLevel();
        }
        retrieveData(qualityLevel);
        delete msg;
    }
    else if (msg->arrivedOn("fromSensors"))
    {
        /*
         * Message arrives from the sensors and sends it
         * to the car app
         */
        sendDataToApp(msg);
    }
    else
    {
        delete msg;
    }
}

/*
 * This method contacts the sensors to ask them all the data
 * that it's ready
 */
void SensorManager::retrieveData(int qualityLevel)
{
    frameId++;

    int sensorsNumber = gateSize("toSensors");

    for (int i = 0; i < sensorsNumber; i++)
    {
        auto packet = new Packet("SensorReq");
        auto request = makeShared<SensorDataRequest>();

        request->setRequestTime(simTime());
        // Every sensor is told the level; only the ones with a quality ladder
        // configured (today the cameras) actually act on it.
        request->setQualityLevel(qualityLevel);
        request->setChunkLength(B(8));
        packet->insertAtBack(request);
        send(packet, "toSensors", i);
    }
}

/*
 * It ads a fake stream id to know from which sensor arrives the
 * data
 */
void SensorManager::sendDataToApp(cMessage* msg)
{
    Packet *packet = dynamic_cast<Packet*>(msg);

    /*
     * If the packet is null then it can be deleted and
     * no operation is done: it's not sent to the car
     */
    if (packet == nullptr)
    {
        delete msg;
        return;
    }

    int idx = packet->getArrivalGate()->getIndex();
    int streamId = sensors[idx].streamId;
    packet->addTagIfAbsent<QuicStreamReq>()->setStreamID(streamId);

    EV_INFO << "Inoltro dati sensore[" << idx << "] su stream " << streamId << endl;
    send(packet, "toApp");
}
