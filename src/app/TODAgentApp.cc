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

TODAgentApp::~TODAgentApp(){}

void TODAgentApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        agentId = par("agentId").stdstringValue();
        carlaCommunicationManager = check_and_cast<CarlaCommunicationManager*>(
                getParentModule()->getParentModule()->getSubmodule("carlaCommunicationManager"));

    }


    //if (stage == INITSTAGE_APPLICATION_LAYER){}
}


void TODAgentApp::handleMessageWhenUp(cMessage* msg){
    if (socket.belongsToSocket(msg)){
        socket.processMessage(msg);
    }

}

void TODAgentApp::refreshDisplay() const{}

void TODAgentApp::finish()
{
    ApplicationBase::finish();
}

void TODAgentApp::handleStartOperation(LifecycleOperation *operation)
{
    L3Address localAddress;
    L3AddressResolver().tryResolve(par("localAddress"), localAddress);
    int localPort = par("localPort");
    socket.setOutputGate(gate("socketOut"));
    socket.bind(localAddress, localPort);
    socket.setCallback(this);
}


void TODAgentApp::handleStopOperation(LifecycleOperation *operation)
{
    socket.close();
}

void TODAgentApp::handleCrashOperation(LifecycleOperation *operation)
{
    if (operation->getRootModule() != getContainingNode(this)) // closes socket when the application crashed only
        socket.destroy(); // TODO  in real operating systems, program crash detected by OS and OS closes sockets of crashed programs.
    socket.setCallback(nullptr);
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


void TODAgentApp::socketClosed(UdpSocket *socket){}

void TODAgentApp::sendPacket(Packet *packet, L3Address address, int port){
    emit(packetSentSignal, packet);
    socket.sendTo(packet, address, port);
    numSent++;
}


bool TODAgentApp::reassembleStatusPacket(string actorId, string statusId, int numFragments){
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


void TODAgentApp::handleStatusUpdateMessage(Packet *statusPacket){
    auto todStatusMessage = statusPacket->peekData<TodStatusUpdateMessage>();
    auto actorId = todStatusMessage->getActorId();
    auto statusId = todStatusMessage->getStatusId();
    auto numFragments = todStatusMessage->getTotalFragments();
    auto fragmentNum = todStatusMessage->getFragmentNum();


    EV_INFO << "Received fragment for "<< actorId << " Status "<< statusId << "(" << fragmentNum +1<< "/" << numFragments << ")"<< endl;

    if (reassembleStatusPacket(actorId, statusId, numFragments )){
        auto srcAddress = statusPacket->getTag<L3AddressInd>()->getSrcAddress();
        auto srcPort = statusPacket->getTag<L4PortInd>()->getSrcPort();
        auto statusCreationTime = statusPacket->peekData<TodStatusUpdateMessage>()->getAllTags<CreationTimeTag>()[0].getTag()->getCreationTime();
        EV_INFO << "handleStatusUpdateMessage " << actorId << "," << statusId << endl;

        auto instructionId = carlaCommunicationManager->computeInstruction(actorId, statusId, agentId);

        auto packet = new Packet("Instruction");
        auto data = makeShared<TodInstructionMessage>();
        // Data
        data->setChunkLength(B(par("instructionMessageLength").intValue()));
        data->setActorId(actorId);
        data->setInstructionId(instructionId.c_str());
        data->setStatusCrationTime(statusCreationTime);
        auto creationTimeTag = data->addTag<CreationTimeTag>(); // add new tag
        creationTimeTag->setCreationTime(simTime()); // store current time

        packet->insertAtBack(data);

        sendPacket(packet, srcAddress, srcPort);
    }
}


void TODAgentApp::processPacket(Packet *packet){
    if (packet->hasData<TODMessage>()){
        if (packet->peekData<TODMessage>()->getMessageType() == TODMessageType::STATUS){
            //CARLA apply instruction
            handleStatusUpdateMessage(packet);
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
