#ifndef __TODCARLANETMANAGER_H_
#define __TODCARLANETMANAGER_H_

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
#include "carlanet/CarlanetManager.h"

#include "TodCarlaApi.h"
#include "TodCarlaInetMobility.h"

using namespace std;
using namespace omnetpp;
using namespace inet;



class TodCarlanetManager: public CarlanetManager {
public:
//    TodCarlanetManager();
//    ~TodCarlanetManager();

    //API used by applications
    string getActorStatus(string actorId);
    string computeInstruction(string actorId, string statusId, string agentId);
    void applyInstruction(string actorId, string instructionId);

protected:
    virtual const map<string,cValue>& getExtraInitParams() override;
    void finish() override;
};

#endif
