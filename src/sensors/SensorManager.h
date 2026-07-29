#ifndef SENSORS_SENSORMANAGER_H_
#define SENSORS_SENSORMANAGER_H_

#include <omnetpp.h>
#include <vector>
#include "inet/common/packet/Packet.h"

using namespace omnetpp;
using namespace inet;

class SensorManager : public cSimpleModule {
protected:
    struct SensorInfo {
        int streamId;
        std::string type;
    };

    std::vector<SensorInfo> sensors; // Vector of sensors: filled during init

    uint64_t frameId = 0;

    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;

    void registerSensors();                     // Read sensors from the input gate
    void retrieveData(int qualityLevel);        // Send a request to each sensor
    void sendDataToApp(cMessage* msg);          // Send data to todCarApp after retrieving

public:
    virtual ~SensorManager();
};

#endif /* SENSORS_SENSORMANAGER_H_ */
