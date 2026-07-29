#ifndef SENSORS_SENSORBASEAPP_H_
#define SENSORS_SENSORBASEAPP_H_

#include <omnetpp.h>
#include <string>
#include <vector>
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

    /*
     * Quality ladder. Each rung is a resolution; the bytes emitted scale with the
     * pixel count relative to rung 0, because for the network a camera frame IS
     * its byte count. Empty ladder (lidar, radar) means the sensor ignores the
     * level and keeps emitting at its nominal dataSize.
     */
    struct QualityRung {
        int width = 0;
        int height = 0;
        double pixelScale = 1.0;   // (w*h) / (w0*h0)
    };
    std::vector<QualityRung> qualityLadder;
    int currentQualityLevel = 0;   // level of the last poll, echoed back in the response

    virtual void initialize() override;
    virtual void handleMessage(cMessage* msg) override;

    virtual void infoFromPacket(cMessage* msg);     // retrieve data from the packets
    virtual void sendData();                        // send the data to the sensor manger

    virtual void parseQualityLadder(const char *spec);  // "1280x720 960x540 ..." -> rungs
    virtual double qualityScale(int level) const;       // byte scale factor for a level

public:
    virtual ~SensorBaseApp();
};

#endif /* SENSORS_SENSORBASEAPP_H_ */
