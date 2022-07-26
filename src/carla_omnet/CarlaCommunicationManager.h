#ifndef __CARLACOMMUNICATIONMANAGER_H_
#define __CARLACOMMUNICATIONMANAGER_H_

#include <string>
#include <list>
#include <zmq.hpp>
#include <iostream>
#include <chrono>
#include <thread>

#include <map>
#include <memory>
#include <list>
#include <queue>
#include <fstream>
#include <nlohmann/json.hpp>

#include "omnetpp.h"
#include "inet/common/INETDefs.h"
#include "CarlaApi.h"
#include "utils.h"

using namespace std;
using namespace omnetpp;
using namespace inet;



class CarlaCommunicationManager: public cSimpleModule {//, public cISimulationLifecycleListener {
public:
    CarlaCommunicationManager();
    ~CarlaCommunicationManager();

    void connect();
    bool isConnected() const
    {
        return true; //static_cast<bool>(connection);
    }



protected:
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;

private:
    void doSimulationTimeStep();
    void initializeCarla();
    list<carla_api_base::init_actor> getModulesToTrack();

    void sendToCarla(json msg);

    template<typename T> T receiveFromCarla();

    bool connection;
    string protocol;
    string host;
    double simulationTimeStep;
    int seed;
    int port;
    zmq::context_t context;
    zmq::socket_t socket;

    cMessage *simulationTimeStepEvent =  new cMessage("simulationTimeStep");
};

#endif
