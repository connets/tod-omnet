#include "TodCarlanetManager.h"


using namespace inet;
using namespace std;

Define_Module(TodCarlanetManager);



/*const map<string,cValue>& TodCarlanetManager::getExtraInitParams(){
    auto extraInitParams = new cValueMap();
    extraInitParams->set("carla_world_configuration",  cValue(par("carlaConfiguration").stdstringValue()));
    return extraInitParams->getFields();
}*/

const std::map<std::string,cValue>& TodCarlanetManager::getExtraInitParams(){
    return check_and_cast<cValueMap*>(par("extraInitParams").objectValue())->getFields();
}


/*
 * PUBLIC APIs
 * */

string TodCarlanetManager::getActorStatus(string actorId){
    EV_INFO << "Contact Carla for getting the status id: "<< actorId << endl;
    tod_carla_api::actor_status_update requestMsg;
    requestMsg.actor_id = actorId;

    tod_carla_api::actor_status response = sendToAndGetFromCarla_actor_generic_message<tod_carla_api::actor_status_update, tod_carla_api::actor_status>(requestMsg);


    return response.status_id;
}

string TodCarlanetManager::computeInstruction(string actorId, string statusId, string agentId){
    EV_INFO << "Contact Carla for getting the instruction id" << endl;

    tod_carla_api::compute_instruction requestMsg;
    requestMsg.actor_id = actorId;
    requestMsg.agent_id = agentId;
    requestMsg.status_id = statusId;

    //json j = requestMsg;
    tod_carla_api::instruction response = sendToAndGetFromCarla_agent_generic_message<tod_carla_api::compute_instruction,tod_carla_api::instruction>(requestMsg);

    return response.instruction_id;
}

void TodCarlanetManager::applyInstruction(string actorId, string instructionId){
    EV_INFO << "Contact Carla for applying the instruction id" << endl;
    tod_carla_api::apply_instruction requestMsg;
    requestMsg.actor_id = actorId;
    requestMsg.instruction_id = instructionId;

    sendToAndGetFromCarla_actor_generic_message<tod_carla_api::apply_instruction, tod_carla_api::ok>(requestMsg);


}

void TodCarlanetManager::sendStatusToActor(string actorId, string statusId){

    //TODO: implement this following zerodelay network COOPERATIVE_UPDATE.json
    EV_INFO << "send to Carla for getting the instruction id" << endl;

    tod_carla_api::cooperative_update requestMsg;
    requestMsg.actor_id = actorId;
    requestMsg.status_id = statusId;

    sendToAndGetFromCarla_actor_generic_message<tod_carla_api::cooperative_update,tod_carla_api::ok>(requestMsg);
}


void CarlanetManager::finish() {
    std::cout << "finish(), before end the simulation" << endl;
}
