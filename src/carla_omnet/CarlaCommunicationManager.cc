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

#include "CarlaInetMobility.h"

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
        connect();
    }

    if (stage == INITSTAGE_SINGLE_MOBILITY){
        initializeCarla();
    }
}

list<carla_api_base::init_actor> CarlaCommunicationManager::getModulesToTrack(){
    auto actorList = list<carla_api_base::init_actor>();
    auto rootModule = getModuleByPath("<root>");
    auto mobilityModules = getSubmodulesOfType<CarlaInetMobility>(rootModule, true);
    for (auto mobilityMod : mobilityModules) {
        auto nodeModule = mobilityMod->getParentModule();
        carla_api_base::init_actor item;
        item.vehicle_id = nodeModule->getFullName();
        item.vehicle_type = mobilityMod->par("carlaActorType").str();
        actorList.push_back(item);
//        inetmm->preInitialize(nodeId, inet::Coord(position.x, position.y), road_id, speed, heading.getRad());
    }

    return actorList;
}


void CarlaCommunicationManager::initializeCarla(){
    carla_api::init msg;
    msg.payload.carla_configuration = par("carlaConfiguration").str();
    msg.payload.carla_timestep = simulationTimeStep;
    msg.payload.seed = 0; //TODO from config
    msg.payload.actors = getModulesToTrack();
    msg.timestamp = simTime().dbl();

    json jsonMsg = msg;
    sendToCarla(jsonMsg);
    // I expect to receive INIT_COMPLETE message
    carla_api::init_completed response = receiveFromCarla<carla_api::init_completed>();
    // Carla informs about the intial timestamp, so I schedule the first similation step at that timestamp
    EV << "Initialization completed" << response.payload.initial_timestamp <<  endl;

    scheduleAt(simTime() + response.payload.initial_timestamp, simulationTimeStepEvent);
}


void CarlaCommunicationManager::doSimulationTimeStep(){
//    float ts = simTime().dbl();
//    EV_INFO << "CarlaCommunicationManager: " << ts;
//    const std::string data{"{\"request_type\":\"simulation_step\", \"timestamp\":" + std::to_string(ts) + "}"};
//    int len = sizeof(data);
//    this->socket.send(zmq::buffer(data), zmq::send_flags::none);
//    zmq::message_t reply{};
//    if (!socket.recv(reply, zmq::recv_flags::none)){
//        std::cout << "ERRORE";
//    }
//    json j = json::parse(reply.to_string());
//    auto a = j.get<sta::simulation_step_answer>();

}


//rma::receive_message_answer CarlaCommunicationManager::receiveMessage(int msg_id){
//    float ts = simTime().dbl();
//    const std::string data{"{\"request_type\":\"receive_msg\", \"timestamp\":" + std::to_string(ts) + ", \"msg_id\":" + std::to_string(msg_id) + "}"};
//    int len = sizeof(data);
//    this->socket.send(zmq::buffer(data), zmq::send_flags::none);
//    zmq::message_t reply{};
//    if (!socket.recv(reply, zmq::recv_flags::none)){
//        std::cout << "ERRORE";
//    }
//    json j = json::parse(reply.to_string());
//    rma::receive_message_answer a = j.get<rma::receive_message_answer>();
//    return a;
//}

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
            EV_INFO << "HERE : =>  " << this->simulationTimeStep << endl;
            scheduleAt(simTime() + this->simulationTimeStep, msg);
        }
    }
}

