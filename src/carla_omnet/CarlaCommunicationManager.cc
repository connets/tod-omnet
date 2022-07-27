#include "CarlaCommunicationManager.h"
#include "inet/applications/base/ApplicationPacket_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/TagBase_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"

#include "../app/TODCarApp.h"

using namespace inet;
using namespace std;

Define_Module(CarlaCommunicationManager);

CarlaCommunicationManager::CarlaCommunicationManager(){

}
CarlaCommunicationManager::~CarlaCommunicationManager(){

}

void CarlaCommunicationManager::sendToCarla(json jsonMsg){
    std::stringstream msg;
    msg << jsonMsg;
    socket.send(zmq::buffer(msg.str()), zmq::send_flags::none);
}

template <typename T> T CarlaCommunicationManager::receiveFromCarla(){
    zmq::message_t reply{};
    if (!socket.recv(reply, zmq::recv_flags::none)){
        EV_ERROR << "receive error"<<endl;
    }
    json j = json::parse(reply.to_string());
    return j.get<T>();
}


void CarlaCommunicationManager::initialize(int stage)
{
    cSimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL){
        protocol = par("protocol").stringValue();
        host = par("host").stringValue();
        port = par("port");
        simulationTimeStep = par("simulationTimeStep");
        findModulesToTrack();
        connect();
    }

    if (stage == INITSTAGE_SINGLE_MOBILITY){
        initializeCarla();
    }
}

void CarlaCommunicationManager::findModulesToTrack(){
    auto rootModule = getModuleByPath("<root>");
    auto mobilityModList = getSubmodulesOfType<CarlaInetMobility>(rootModule, true);
    for (auto mobilityMod : mobilityModList) {
        string nodeModuleName = mobilityMod->getParentModule()->getFullName();
        modulesToTrack.insert(pair<string, CarlaInetMobility*>(nodeModuleName, mobilityMod));
    }
}


void CarlaCommunicationManager::initializeCarla(){
    // conversion
    auto actorList = list<carla_api_base::init_actor>();
    for (auto elem : modulesToTrack) {
        auto mobilityMod = elem.second;
        auto nodeModule = mobilityMod->getParentModule();
        carla_api_base::init_actor item;

        item.actor_id = elem.first;
        item.actor_configuration = mobilityMod->par("carlaActorType").str();
        item.route = mobilityMod->par("route").str();
        // Check if TOD application exists
        auto agentList = list<carla_api_base::init_agent>();
//        auto nodeApplications = nodeModule->getSubmodule("app");
        for (cModule::SubmoduleIterator it(nodeModule); !it.end(); it++) {
            cModule *submodule = *it;
            if (strstr(submodule->getNedTypeName(),"TODCarApp") != nullptr){
                carla_api_base::init_agent agent;
                agent.agent_id = elem.first; // The same id of actor
                agent.agent_configuration = submodule->par("agentConfiguration").str();
                agentList.push_back(agent);
            }
            //EV << submodule->getFullName() << endl;
        }
//        if (nodeApplications!= nullptr){
//            for (int i = 0; i < nodeApplications->getVectorSize(); i++){
//                auto appModule = nodeModule->getSubmodule("app",i);
//                if (appModule->getNedTypeName() == "TODAgentApp"){
//                    carla_api_base::init_agent agent;
//                    agent.agent_id = elem.first; // The same id of actor
//                    agent.agent_configuration = appModule->par("agentConfiguration").str();
//                    agentList.push_back(agent);
//                }
//            }
//        }
        item.agents = agentList;
        actorList.push_back(item);
    }

    // compose the message
    carla_api::init msg;
    msg.payload.carla_configuration = par("carlaConfiguration").str();
    msg.payload.carla_timestep = simulationTimeStep;
    msg.payload.seed = 0; //TODO from config
    msg.payload.actors = actorList;
    msg.timestamp = simTime().dbl();

    json jsonMsg = msg;
    sendToCarla(jsonMsg);
    // I expect to receive INIT_COMPLETE message
    carla_api::init_completed response = receiveFromCarla<carla_api::init_completed>();
    // Carla informs about the intial timestamp, so I schedule the first similation step at that timestamp
    EV << "Initialization completed" << response.payload.initial_timestamp <<  endl;
    //
    initial_timestamp = simTime() + response.payload.initial_timestamp;
    // schedule
    scheduleAt(simTime() + response.payload.initial_timestamp, simulationTimeStepEvent);
}


void CarlaCommunicationManager::doSimulationTimeStep(){
    carla_api::simulation_step msg;
    msg.payload.timestep = simulationTimeStep;
    json jsonMsg = msg;
    sendToCarla(jsonMsg);
    // I expect updated_postion message
    carla_api::updated_postion response = receiveFromCarla<carla_api::updated_postion>();

    //Update position of all nodes in response
    for(auto actor : response.payload.actors){
        EV << "Position" << actor.position <<endl;
        Coord position = Coord(actor.position[0], actor.position[1], actor.position[2]);
        Coord velocity = Coord(actor.velocity[0],actor.velocity[1],actor.velocity[2]);
        Quaternion rotation = Quaternion(EulerAngles(rad(actor.rotation[0]),rad(actor.rotation[1]),rad(actor.rotation[2])));
        modulesToTrack[actor.actor_id]->nextPosition(position, velocity, rotation);
    }

}


void CarlaCommunicationManager::connect(){
    this->context = zmq::context_t {1};
    this->socket = zmq::socket_t{context, zmq::socket_type::req};
    int timeout_ms = 10000;
    this->socket.setsockopt(ZMQ_RCVTIMEO, timeout_ms); // set timeout to value of timeout_ms
    this->socket.setsockopt(ZMQ_SNDTIMEO, timeout_ms); // set timeout to value of timeout_ms
    EV_INFO << "CarlaCommunicationManagerLog " << "Finish initialize" << endl;
    string addr = protocol + "://" + host + ":" + std::to_string(port);
    EV << "Trying connecting to: " << addr << endl;
    socket.connect(addr);
}

void CarlaCommunicationManager::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()){
        if (msg == simulationTimeStepEvent){
            doSimulationTimeStep();
            EV_INFO << "Simulation step: " << this->simulationTimeStep << endl;
            scheduleAt(simTime() + this->simulationTimeStep, msg);
        }
    }
}


/*
 * PUBLIC APIs
 * */

string CarlaCommunicationManager::getActorStatus(string actorId){
    EV_INFO << "Contact Carla for getting the status id" << endl;
    return "test";
}

string CarlaCommunicationManager::computeInstruction(string actorId, string statusId, string agentId){
    EV_INFO << "Contact Carla for getting the instruction id" << endl;
    return "test";
}

void CarlaCommunicationManager::applyInstruction(string actorId, string instructionId){
    EV_INFO << "Contact Carla for applyingg the instruction id" << endl;
}


