//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004,2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "TODCarApp.h"

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

using namespace omnetpp;
using namespace inet;

Define_Module(TODCarApp);

void StatusCreationTime::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details){
    auto packet = check_and_cast<Packet*>(object);
    simtime_t retrievalTime = packet->peekData<TodStatusUpdateMessage>()->getCollectionTime();

    auto instrucionDelay = simTime() - retrievalTime;

    fire(this, simTime(), instrucionDelay,  details );
}


void instructionRTTNetworkFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details){
    auto packet = check_and_cast<Packet*>(object);
    auto instructionMessage = packet->peekData<TodInstructionMessage>();
    auto uplinkRtt = instructionMessage->getStatusProcessingTime() - instructionMessage->getStatusCreationTime();
    auto downLinkRtt = simTime() - instructionMessage->getInstructionCreationTime();

    auto rtt = downLinkRtt + uplinkRtt;

    fire(this, simTime(), rtt,  details );
}

void InstructionDelayResultFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details){
    auto packet = check_and_cast<Packet*>(object);
    simtime_t retrievalTime = packet->peekData<TodInstructionMessage>()->getStatusDataCollectionTime();

    auto instrucionDelay = simTime() - retrievalTime;

    fire(this, simTime(), instrucionDelay,  details );
}



TODCarApp::~TODCarApp()
{
    cancelAndDelete(updateStatusSelfMessage);
}

void TODCarApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        actorId = getParentModule()->getName();
        carlaCommunicationManager = check_and_cast<TodCarlanetManager*>(
                getParentModule()->getParentModule()->getSubmodule("carlaCommunicationManager"));

        updateStatusSelfMessage = new cMessage("UpdateStatus");
        statusUpdateInterval = par("statusUpdateInterval");
        EV_INFO << "****** => " << statusUpdateInterval << endl;
    }


    //if (stage == INITSTAGE_APPLICATION_LAYER){}
}

void TODCarApp::refreshDisplay() const{}

void TODCarApp::finish()
{
    ApplicationBase::finish();
}

void TODCarApp::handleStartOperation(LifecycleOperation *operation)
{

    L3AddressResolver().tryResolve(par("destAddress"), destAddress);
    destPort = par("destPort");

    socket.setOutputGate(gate("socketOut"));
    socket.bind(destPort);
    //socket.setTos(0b00011100);
    socket.setCallback(this);

    // wait statusUpdateInterval more before start to let Carla be ready
    simtime_t firstStatusUpdate = simTime() + carlaCommunicationManager->getCarlaInitialCarlaTimestamp() + statusUpdateInterval;

    EV_INFO << "First update will be at: " << firstStatusUpdate << endl;

    scheduleAt(firstStatusUpdate, updateStatusSelfMessage);
}


void TODCarApp::handleStopOperation(LifecycleOperation *operation)
{
    socket.close();
}

void TODCarApp::handleCrashOperation(LifecycleOperation *operation)
{
    if (operation->getRootModule() != getContainingNode(this)) // closes socket when the application crashed only
        socket.destroy(); // TODO  in real operating systems, program crash detected by OS and OS closes sockets of crashed programs.
    socket.setCallback(nullptr);
}


void TODCarApp::handleMessageWhenUp(cMessage* msg){

    if (msg->isSelfMessage()){

        if (msg == updateStatusSelfMessage){
            retrieveStatusData();
            //sendUpdateStatusPacket();
            // this time contains all the parameters needed to generate status message, TODO: create ad hoc message
            // Note, you have to add the same time for all the UDP pkt of one frame
            scheduleAfter(statusUpdateInterval, msg);

        }
        else if (msg->getKind() == CREATION_STATUS_DATA_MSG_KIND) {
            sendUpdateStatusPacket(simTime());
        }
    }else if(socket.belongsToSocket(msg)){
            socket.processMessage(msg);
    }

}




void TODCarApp::retrieveStatusData(){

    double encTime = par("encodingImageTime");
    double collectTime = par("collectionDataTime");
    double creationStatusTime = encTime + collectTime;

    cMessage* msg = new cMessage("creationStatusTime", CREATION_STATUS_DATA_MSG_KIND);
    // msg->setTimestamp();
    scheduleAfter(creationStatusTime, msg);
}



void TODCarApp::sendUpdateStatusPacket(simtime_t dataRetrievalTime){
    //get status id form CARLA API

    EV_INFO << "Send status update" << endl;
    string statusId = carlaCommunicationManager->getActorStatus(actorId);

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
    int statusMessageLength = par("statusMessageLength").intValue();
    int numFragments = std::ceil(1.0*statusMessageLength/ (UDP_MAX_MESSAGE_SIZE-10));
    int fragmentNum = 0;
    while (statusMessageLength>0){
        int fragmentLength = std::min(statusMessageLength, (int) UDP_MAX_MESSAGE_SIZE-10);
        EV_INFO << "Send status update FRAGMENT:" << fragmentLength << endl;
        auto packet = new Packet((string("StatusUpdate_")+statusId+"_"+ std::to_string(fragmentNum)).c_str());
        auto data = makeShared<TodStatusUpdateMessage>();

        data->setChunkLength(B(fragmentLength));
        data->setActorId(actorId);
        data->setStatusId(statusId.c_str());
        data->setTotalFragments(numFragments);
        data->setCollectionTime(dataRetrievalTime);
        EV_INFO << "Send status update NUM FRAGMENTS: "<< fragmentNum<< "/"<< numFragments << endl;
        data->setFragmentNum(fragmentNum);

        auto creationTimeTag = data->addTag<CreationTimeTag>(); // add new tag
        creationTimeTag->setCreationTime(simTime()); // store current time
        packet->insertAtBack(data);
        sendPacket(packet);

        statusMessageLength -= UDP_MAX_MESSAGE_SIZE-10;
        fragmentNum++;
    }


}


void TODCarApp::socketDataArrived(UdpSocket *socket, Packet *packet){
    emit(packetReceivedSignal, packet);
    EV_INFO << "Received packet: " << UdpSocket::getReceivedPacketInfo(packet) << endl;

    processPacket(packet);

    delete packet;
    numReceived++;
}


void TODCarApp::socketErrorArrived(UdpSocket *socket, Indication *indication){}


void TODCarApp::socketClosed(UdpSocket *socket){}

void TODCarApp::sendPacket(Packet *packet){
    emit(packetSentSignal, packet);
    socket.sendTo(packet, destAddress, destPort);
    numSent++;
}



void TODCarApp::processPacket(Packet *pk){
    if (pk->hasData<TODMessage>()){
        if (pk->peekData<TODMessage>()->getMessageType() == TODMessageType::INSTRUCTION){
            //CARLA apply instruction
            auto message = pk->peekData<TodInstructionMessage>();
            carlaCommunicationManager->applyInstruction(message->getActorId(), message->getInstructionId());
        }
        else{
            EV_WARN << "Received an unexpected TOD Message " <<  pk->peekData<TODMessage>()->getMessageType()  << " check your implementation"<< endl;
        }
    }
    else{
        EV_WARN << "Received an unexpected packet "<< UdpSocket::getReceivedPacketInfo(pk) <<endl;
    }
    //pk->
    //{
    ////    const auto& received_payload = pk->peekData<TODMessage>();
    ////    rma::receive_message_answer answer = carlaCommunicationManager->receiveMessage(received_payload->getMsgId());
    ////    emit(packetReceivedSignal, pk);
    ////    EV_INFO << "Received packet: " << UdpSocket::getReceivedPacketInfo(pk) << endl;
    ////    delete pk;
    ////    numReceived++;
}
