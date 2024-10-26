//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "CooperativePerceptionCarApp.h"

#include <math.h>


#include "inet/common/ModuleAccess.h"
#include "inet/common/TagBase_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/udp/Udp.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"
//#include "carla_omnet/TodCarlanetManager.h"
#include "messages/TodMessages_m.h"

#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"

Define_Module(CooperativePerceptionCarApp);

CooperativePerceptionCarApp::~CooperativePerceptionCarApp()
{
    cancelAndDelete(updateStatusSelfMessageCoop);
}

void CooperativePerceptionCarApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {

        //actorId = (check_and_cast<TodCarlaInetMobility*>getParentModule()->getSubmodule("mobility"))->getCarlaId();
        auto mobilityModule = check_and_cast<CarlaInetMobility*>(getParentModule()->getSubmodule("mobility"));
        //std::string carlaID = mobilityModule->getCarlaId();
        //std::cout << "CooperativePerceptionCarApp::initialize "<< carlaID << " " <<  endl;
        //actorId = carlaID.c_str();
        //actorId = getParentModule()->getName();

        carlaCommunicationManager = check_and_cast<TodCarlanetManager*>(
                getParentModule()->getParentModule()->getSubmodule("carlaCommunicationManager"));

        updateStatusSelfMessageCoop = new cMessage("UpdateStatusCoop");
        statusUpdateIntervalCoop = par("statusUpdateIntervalCoop");
        EV_INFO << "****** coop => " << statusUpdateIntervalCoop << endl;
    }


    //if (stage == INITSTAGE_APPLICATION_LAYER){}
}

void CooperativePerceptionCarApp::refreshDisplay() const{}

void CooperativePerceptionCarApp::finish()
{
    ApplicationBase::finish();
}

void CooperativePerceptionCarApp::handleStartOperation(LifecycleOperation *operation)
{


    L3AddressResolver().tryResolve(par("destAddressesCoop"), destAddressesCoop);
    destPort = par("destPort");

    std::cout << "CooperativePerceptionCarApp::handleStartOperation "<< destAddressesCoop << ":" << destPort <<  endl;

    socket.setOutputGate(gate("socketOut"));
    socket.bind(destPort);
    //socket.setTos(0b00011100);
    socket.setCallback(this);

    // wait statusUpdateInterval more before start to let Carla be ready
    simtime_t firstStatusUpdateCoop = simTime() + carlaCommunicationManager->getCarlaInitialCarlaTimestamp() + statusUpdateIntervalCoop;

    EV_INFO << "First update for coop will be at: " << firstStatusUpdateCoop << endl;

    scheduleAt(firstStatusUpdateCoop, updateStatusSelfMessageCoop);
}


void CooperativePerceptionCarApp::handleStopOperation(LifecycleOperation *operation)
{
    socket.close();
}

void CooperativePerceptionCarApp::handleCrashOperation(LifecycleOperation *operation)
{
    if (operation->getRootModule() != getContainingNode(this)) // closes socket when the application crashed only
        socket.destroy(); // TODO  in real operating systems, program crash detected by OS and OS closes sockets of crashed programs.
    socket.setCallback(nullptr);
}


void CooperativePerceptionCarApp::handleMessageWhenUp(cMessage* msg){

    if (msg->isSelfMessage()){

        if (msg == updateStatusSelfMessageCoop){
            retrieveStatusDataCoop();
            scheduleAfter(statusUpdateIntervalCoop, msg);
        }
        else if (msg->getKind() == CREATION_STATUS_DATA_MSG_KIND_COOP) {
            //create status
            sendUpdateStatusPacketCoop(simTime());
        } else if (msg->getKind() == PROCESS_STATUS_MESSAGE_KIND) {
            //received status
            ProcessedStatusMessage *todStatusMessage = dynamic_cast<ProcessedStatusMessage *>(msg);
            auto actorIdFrom = todStatusMessage->getActorId();
            auto statusId = todStatusMessage->getStatusId();

            EV_INFO << "handleStatusUpdateMessage " << actorIdFrom << "," << statusId << endl;

            //sendCooperativeStatusToActor in carla
            carlaCommunicationManager->sendStatusToActor(actorIdFrom, statusId);
        }

    }else if(socket.belongsToSocket(msg)){
            socket.processMessage(msg);
    }

}

void CooperativePerceptionCarApp::retrieveStatusDataCoop(){

    double encTime = par("encodingImageTime");
    double collectTime = par("collectionDataTime");
    double creationStatusTime = encTime + collectTime;

    cMessage* msg = new cMessage("creationStatusTime", CREATION_STATUS_DATA_MSG_KIND_COOP);
    // msg->setTimestamp();
    scheduleAfter(creationStatusTime, msg);
}



void CooperativePerceptionCarApp::sendUpdateStatusPacketCoop(simtime_t dataRetrievalTime){

    L3AddressResolver().tryResolve(par("destAddressesCoop"), destAddressesCoop);

    //get status id form CARLA API
    auto mobilityModule = check_and_cast<CarlaInetMobility*>(getParentModule()->getSubmodule("mobility"));
    std::string carlaID = mobilityModule->getCarlaId();

    EV_INFO << "Send status update for coop for id: "<< carlaID << " to: "<< destAddressesCoop<<":"<<destPort<< endl;
    string statusId = carlaCommunicationManager->getActorStatus(carlaID);

//    data->setChunkLength(B(1));
//    data->setActorId(actorId);
//    data->setStatusId(statusId.c_str());
//    data->setTotalFragments(1);
//    data->setFragmentNum(1);
//
//    auto creationTimeTag = data->addTag<CreationTimeTag>(); // add new tag
//    creationTimeTag->setCreationTime(simTime()); // store current time
//    packet->insertAtBack(data);
//
//    auto dataByte = makeShared<ByteCountChunk>(B(statusMessageLength));
//    packet->insertAtBack(dataByte);
//
//    sendPacket(packet);

    // Data
    int statusMessageLengthCoop = par("statusMessageLengthCoop").intValue();
    int numFragments = std::ceil(1.0*statusMessageLengthCoop/ (UDP_MAX_MESSAGE_SIZE-10));
    int fragmentNum = 0;
    while (statusMessageLengthCoop>0){
        int fragmentLength = std::min(statusMessageLengthCoop, (int) UDP_MAX_MESSAGE_SIZE-10);
        EV_INFO << "Send status update cooperative perception FRAGMENT:" << fragmentLength << endl;
        auto packet = new Packet((string("StatusUpdate_CoopPerc_")+statusId+"_"+ std::to_string(fragmentNum)).c_str());

        //TODO: CHANGE TodStatusUpdateMessage
        auto data = makeShared<TodStatusUpdateMessage>();

        data->setChunkLength(B(fragmentLength));
        data->setActorId(carlaID.c_str());
        data->setStatusId(statusId.c_str());
        data->setTotalFragments(numFragments);
        data->setCollectionTime(dataRetrievalTime);
        EV_INFO << "Send status update NUM FRAGMENTS: "<< fragmentNum<< "/"<< numFragments << endl;
        data->setFragmentNum(fragmentNum);

        auto creationTimeTag = data->addTag<CreationTimeTag>(); // add new tag
        creationTimeTag->setCreationTime(simTime()); // store current time
        packet->insertAtBack(data);
        sendPacket(packet, destAddressesCoop);

        statusMessageLengthCoop -= UDP_MAX_MESSAGE_SIZE-10;
        fragmentNum++;
    }


}


void CooperativePerceptionCarApp::socketDataArrived(UdpSocket *socket, Packet *packet){
    emit(packetReceivedSignal, packet);
    EV_INFO << "Received packet: " << UdpSocket::getReceivedPacketInfo(packet) << endl;

    processPacket(packet);

    delete packet;
}


void CooperativePerceptionCarApp::socketErrorArrived(UdpSocket *socket, Indication *indication){}


void CooperativePerceptionCarApp::socketClosed(UdpSocket *socket){}

void CooperativePerceptionCarApp::sendPacket(Packet *packet, L3Address dsts){
    emit(packetSentSignal, packet);
    socket.sendTo(packet, dsts, destPort);
}


/*Cooperative Percetion handling receive messaage*/
bool CooperativePerceptionCarApp::reassembleStatusPacket(string actorId, string statusId, int numFragments){
    if (numFragments == 1){
        return true;
    }

    auto key = pair<string,string>(actorId, statusId);
    auto it = reassembleStatusPacketsMap.find(key);
    if(it == reassembleStatusPacketsMap.end()){
        // new packet
        int value = numFragments - 1;
        reassembleStatusPacketsMap.insert(pair<pair<string, string>,int>(key,value));
        return false;
    }
    else {
        // another fragment
        if (reassembleStatusPacketsMap[key] == 1){
            reassembleStatusPacketsMap.erase(key);
            return true;
        }
        else{
            reassembleStatusPacketsMap[key] = reassembleStatusPacketsMap[key] - 1;
            return false;
        }

    }

}

void CooperativePerceptionCarApp::handleStatusUpdateMessage(Packet *statusPacket){
    auto todStatusMessage = statusPacket->peekData<TodStatusUpdateMessage>();
    auto actorId = todStatusMessage->getActorId();
    auto statusId = todStatusMessage->getStatusId();
    auto numFragments = todStatusMessage->getTotalFragments();
    auto fragmentNum = todStatusMessage->getFragmentNum();

    EV_INFO << "Received fragment for "<< actorId << " Status "<< statusId << "(" << fragmentNum +1<< "/" << numFragments << ")"<< endl;


    if (reassembleStatusPacket(actorId, statusId, numFragments )){
        ProcessedStatusMessage *pkt = new ProcessedStatusMessage("processStatusMessage", PROCESS_STATUS_MESSAGE_KIND);
        pkt->setActorId(todStatusMessage->getActorId());
        pkt->setStatusId(todStatusMessage->getStatusId());
        pkt->setStatusCreationTime(statusPacket->peekData<TodStatusUpdateMessage>()->getAllTags<CreationTimeTag>()[0].getTag()->getCreationTime());
        pkt->setCollectionTime(todStatusMessage->getCollectionTime());
        pkt->setSrcAddress(statusPacket->getTag<L3AddressInd>()->getSrcAddress());
        pkt->setSrcPort(statusPacket->getTag<L4PortInd>()->getSrcPort());
        pkt->setTimestamp();

        double processingStatusTime = par("processingStatusTime");
        scheduleAfter(processingStatusTime, pkt);
    }
}
/*End*/


void CooperativePerceptionCarApp::processPacket(Packet *pk){

    EV_INFO << "handle arrived packet Cooperative perception" << endl;

    if (pk->hasData<TODMessage>()){
        if (pk->peekData<TODMessage>()->getMessageType() == TODMessageType::STATUS){
            //receive message of status from others cars, send to CARLA ENVIRONMENT
            handleStatusUpdateMessage(pk);
        }
    }
    else{
        EV_WARN << "Received an unexpected packet "<< UdpSocket::getReceivedPacketInfo(pk) <<endl;
    }

}
