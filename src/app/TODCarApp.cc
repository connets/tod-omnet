//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004,2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "TODCarApp.h"

#include "inet/common/ModuleAccess.h"
#include "inet/common/TagBase_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"
#include "../carla_omnet/CarlaCommunicationManager.h"
#include "messages/TodMessages_m.h"

using namespace omnetpp;
using namespace inet;

Define_Module(TODCarApp);

TODCarApp::~TODCarApp()
{
    socket.destroy();
    delete updateStatusSelfMessage;
}

void TODCarApp::initialize(int stage)
{
    cModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        actorId = getParentModule()->getName();
        carlaCommunicationManager = check_and_cast<CarlaCommunicationManager*>(
                getParentModule()->getParentModule()->getSubmodule("carlaCommunicationManager"));

        updateStatusSelfMessage = new cMessage("UpdateStatus");
        statusUpdateInterval = par("statusUpdateInterval");
    }


    if (stage == INITSTAGE_APPLICATION_LAYER){
        L3AddressResolver().tryResolve(par("destAddress"), destAddress);
        destPort = par("destPort");
        socket.setOutputGate(gate("socketOut"));
        socket.bind(destPort);
        socket.setTos(0b00011100);
        socket.setCallback(this);
        // wait statusUpdateInterval more before start to let Carla be ready
        simtime_t firstStatusUpdate = simTime()+ carlaCommunicationManager->getCarlaInitialCarlaTimestamp() + statusUpdateInterval;

        EV_INFO << "First update will be at: " << firstStatusUpdate << endl;

        scheduleAt(firstStatusUpdate, updateStatusSelfMessage);
    }
}

void TODCarApp::handleMessage(cMessage* msg){
    if (msg->isSelfMessage()){
        if (msg == updateStatusSelfMessage){
            sendUpdateStatusPacket();
            scheduleAt(simTime()+statusUpdateInterval, msg);
        }
    }else{
        if(socket.belongsToSocket(msg)){
            socket.processMessage(msg);
        }
    }

}


void TODCarApp::sendUpdateStatusPacket(){
    //get status id form CARLA API

    EV_INFO << "Send status update" << endl;
    string statusId = carlaCommunicationManager->getActorStatus(actorId);

    auto packet = new Packet("StatusUpdate");
    auto data = makeShared<TodStatusUpdateMessage>();
    // Data
    data->setChunkLength(B(par("statusMessageLength")));
    data->setActorId(actorId);
    data->setStatusId(statusId.c_str());
    auto creationTimeTag = data->addTag<CreationTimeTag>(); // add new tag
    creationTimeTag->setCreationTime(simTime()); // store current time

    packet->insertAtBack(data);

    sendPacket(packet);
}



void TODCarApp::socketDataArrived(UdpSocket *socket, Packet *packet){
    emit(packetReceivedSignal, packet);
    EV_INFO << "Received packet: " << UdpSocket::getReceivedPacketInfo(packet) << endl;

    processPacket(packet);

    delete packet;
    numReceived++;
}


void TODCarApp::socketErrorArrived(UdpSocket *socket, Indication *indication){

}


void TODCarApp::socketClosed(UdpSocket *socket){

}

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
