#include "TodCarlanetManager.h"


using namespace inet;
using namespace std;

Define_Module(TodCarlanetManager);

// This results in an error when simulation is terminated on carla's side since it's trying to contact an already closed simulator
void TodCarlanetManager::finish(){
    if(this->isConnected()){
        EV_INFO << "Contact Carla for finishing the simulation" << endl;
        tod_carla_api::finish_simulation requestMsg;
        sendToAndGetFromCarla<tod_carla_api::finish_simulation, tod_carla_api::ok>(requestMsg);
    } else {
        EV_INFO << "Connection already closed, skipping finish simulation message" << endl;
    }
}


const map<string,cValue>& TodCarlanetManager::getExtraInitParams(){
    auto extraInitParams = new cValueMap();
    extraInitParams->set("carla_world_configuration",  cValue(par("carlaConfiguration").stdstringValue()));
    return extraInitParams->getFields();
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


