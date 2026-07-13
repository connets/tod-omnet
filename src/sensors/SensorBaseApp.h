#ifndef SENSORS_SENSORBASEAPP_H_
#define SENSORS_SENSORBASEAPP_H_

#include <omnetpp.h>
#include "inet/common/packet/Packet.h"

using namespace omnetpp;
using namespace inet;

class SensorBaseApp : public cSimpleModule {
protected:
    int streamID;               // stream ID for the sensor
    std::string sensorType;     // string with the name of the sensor
    simtime_t updateInterval;   // interval to fetch data from sensor

    uint64_t lastFrameId = 0;   // last frame ID to count
    simtime_t lastEmitTime;     // last time in which the message was sent
    bool firstEmit = true;      // boolean that tells if it's the first message from the sensor

    virtual void initialize() override;
    virtual void handleMessage(cMessage* msg) override;

    virtual void infoFromPacket(cMessage* msg);     // retrieve data from the packets
    virtual void sendData();                        // send the data to the sensor manger

public:
    virtual ~SensorBaseApp();
};

#endif /* SENSORS_SENSORBASEAPP_H_ */
