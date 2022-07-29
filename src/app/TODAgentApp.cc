//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004,2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "TODAgentApp.h"

#include "inet/common/ModuleAccess.h"
#include "inet/common/TagBase_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"

#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"

using namespace omnetpp;
using namespace inet;

Define_Module(TODAgentApp);

TODAgentApp::~TODAgentApp()
{
    socket.destroy();
}

void TODAgentApp::initialize(int stage)
{
    cModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        agentId = par("agentId").str();
        carlaCommunicationManager = check_and_cast<CarlaCommunicationManager*>(
                getParentModule()->getParentModule()->getSubmodule("carlaCommunicationManager"));

    }


    if (stage == INITSTAGE_APPLICATION_LAYER){
        L3Address localAddress;
        L3AddressResolver().tryResolve(par("localAddress"), localAddress);
        int localPort = par("localPort");
        socket.setOutputGate(gate("socketOut"));
        socket.bind(localAddress, localPort);
        socket.setCallback(this);
    }
}


void TODAgentApp::handleMessage(cMessage* msg){
    if (socket.belongsToSocket(msg)){
        socket.processMessage(msg);
    }

}



void TODAgentApp::socketDataArrived(UdpSocket *socket, Packet *packet){
    emit(packetReceivedSignal, packet);
    EV_INFO << "Received packet: " << UdpSocket::getReceivedPacketInfo(packet) << endl;

    processPacket(packet);

    delete packet;
    numReceived++;
}


void TODAgentApp::socketErrorArrived(UdpSocket *socket, Indication *indication){
    EV_INFO << "Socket Error packet: " << indication << endl;
}


void TODAgentApp::socketClosed(UdpSocket *socket){

}

void TODAgentApp::sendPacket(Packet *packet, L3Address address, int port){
    emit(packetSentSignal, packet);
    socket.sendTo(packet, address, port);
    numSent++;
}

void TODAgentApp::handleStatusUpdateMessage(const char* actorId, const char* statusId, L3Address srcAddr, int srcPort){
    EV_INFO << "handleStatusUpdateMessage " << actorId << "," << statusId << endl;
    auto instructionId = carlaCommunicationManager->computeInstruction(actorId, statusId, agentId);

    auto packet = new Packet("Instruction");
    auto data = makeShared<TodInstructionMessage>();
    // Data
    data->setChunkLength(B(par("instructionMessageLength")));
    data->setActorId(actorId);
    data->setInstructionId(instructionId.c_str());
    auto creationTimeTag = data->addTag<CreationTimeTag>(); // add new tag
    creationTimeTag->setCreationTime(simTime()); // store current time

    packet->insertAtBack(data);

    sendPacket(packet, srcAddr, srcPort);
}


void TODAgentApp::processPacket(Packet *packet){
    if (packet->hasData<TODMessage>()){
        if (packet->peekData<TODMessage>()->getMessageType() == TODMessageType::STATUS){
            //CARLA apply instruction
            auto actorId = packet->peekData<TodStatusUpdateMessage>()->getActorId();
            auto instructionId = packet->peekData<TodStatusUpdateMessage>()->getStatusId();
            auto srcAddress = packet->getTag<L3AddressInd>()->getSrcAddress();
            auto srcPort = packet->getTag<L4PortInd>()->getSrcPort();
            handleStatusUpdateMessage(actorId, instructionId, srcAddress, srcPort);
        }
        else{
            EV_WARN << "Received an unexpected TOD Message " <<  packet->peekData<TODMessage>()->getMessageType()  << " check your implementation"<< endl;
        }
    }
    else{
        EV_WARN << "Received an unexpected packet "<< UdpSocket::getReceivedPacketInfo(packet) <<endl;
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
