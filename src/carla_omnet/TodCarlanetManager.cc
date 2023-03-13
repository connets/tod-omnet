#include "carla_omnet/TodCarlanetManager.h"

#include <stdexcept>

#include "inet/applications/base/ApplicationPacket_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/TagBase_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"

#include "app/TODCarApp.h"

using namespace inet;
using namespace std;

Define_Module(TodCarlanetManager);



const map<string,cValue>& TodCarlanetManager::getExtraInitParams(){
    auto extraInitParams = cValueMap();
    extraInitParams.set("carla_world_configuration",  cValue(par("carlaConfiguration").stdstringValue()));
    return extraInitParams.getFields();
}


/*
 * PUBLIC APIs
 * */

string TodCarlanetManager::getActorStatus(string actorId){
    EV_INFO << "Contact Carla for getting the status id" << endl;
    tod_carla_api::actor_status_update requestMsg;
    requestMsg.actor_id = actorId;

    tod_carla_api::actor_status response = sendToAndGetFromCarla<tod_carla_api::actor_status_update, tod_carla_api::actor_status>(requestMsg);


    return response.status_id;
}

string TodCarlanetManager::computeInstruction(string actorId, string statusId, string agentId){
    EV_INFO << "Contact Carla for getting the instruction id" << endl;

    tod_carla_api::compute_instruction requestMsg;
    requestMsg.actor_id = actorId;
    requestMsg.agent_id = agentId;
    requestMsg.status_id = statusId;

    //json j = requestMsg;
    tod_carla_api::instruction response = sendToAndGetFromCarla<tod_carla_api::compute_instruction,tod_carla_api::instruction>(requestMsg);

    return response.instruction_id;
}

void TodCarlanetManager::applyInstruction(string actorId, string instructionId){
    EV_INFO << "Contact Carla for applying the instruction id" << endl;
    tod_carla_api::apply_instruction requestMsg;
    requestMsg.actor_id = actorId;
    requestMsg.instruction_id = instructionId;

    sendToAndGetFromCarla<tod_carla_api::apply_instruction, tod_carla_api::ok>(requestMsg);


}


