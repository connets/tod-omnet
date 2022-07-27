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
#include "CarlaInetMobility.h"

using namespace std;
using namespace omnetpp;
using namespace inet;



class CarlaCommunicationManager: public cSimpleModule {//, public cISimulationLifecycleListener {
public:
    CarlaCommunicationManager();
    ~CarlaCommunicationManager();

    bool isConnected() const
    {
        return true; //static_cast<bool>(connection);
    }

    simtime_t getCarlaInitialCarlaTimestamp() { return initial_timestamp; }

    //API used by applications
    string getActorStatus(string actorId);
    string computeInstruction(string actorId, string statusId, string agentId);
    void applyInstruction(string actorId, string instructionId);


protected:
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;

private:
    void connect();
    void doSimulationTimeStep();
    void initializeCarla();
    void findModulesToTrack();

    void sendToCarla(json msg);

    template<typename T> T receiveFromCarla();

    bool connection;
    string protocol;
    string host;
    double simulationTimeStep;
    simtime_t initial_timestamp = 0;
    int seed;
    int port;
    zmq::context_t context;
    zmq::socket_t socket;

    cMessage *simulationTimeStepEvent =  new cMessage("simulationTimeStep");
    map<string,CarlaInetMobility*> modulesToTrack = map<string,CarlaInetMobility*>();
};

#endif
