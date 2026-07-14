//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004,2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "TODAgentApp.h"

#include "../sensors/messages/SensorMessages_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/TagBase_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/quic/QuicCommand_m.h"

#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"

using namespace omnetpp;
using namespace inet;

Define_Module(TODAgentApp);


void ProcessStatusTimeFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet*>(object);
    simtime_t retrievalTime = packet->peekData<TodInstructionMessage>()->getStatusProcessingTime();

    auto instrucionDelay = simTime() - retrievalTime;

    fire(this, simTime(), instrucionDelay,  details);
}


TODAgentApp::~TODAgentApp()
{
    for (QuicSocket *cs : clientSockets) delete cs;
}

void TODAgentApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        agentId = par("agentId").stdstringValue();
        carlaCommunicationManager = check_and_cast<TodCarlanetManager*>(
                getParentModule()->getParentModule()->getSubmodule("carlaCommunicationManager"));
    }
}


void TODAgentApp::handleMessageWhenUp(cMessage* msg)
{
    if (msg->isSelfMessage()){
        if (msg->getKind() == PROCESS_STATUS_MESSAGE_KIND) {
            ProcessedStatusMessage *pkt = dynamic_cast<ProcessedStatusMessage *>(msg);
            calcAndSendnstruction(pkt);
        }
    }
    else if (socket.belongsToSocket(msg)) socket.processMessage(msg);
    else for (QuicSocket *cs : clientSockets) if (cs->belongsToSocket(msg)) { cs->processMessage(msg); break; }
}


void TODAgentApp::calcAndSendnstruction(ProcessedStatusMessage *todStatusMessage)
{
    auto actorId = todStatusMessage->getActorId();
    auto statusId = todStatusMessage->getStatusId();
    auto statusCreationTime = todStatusMessage->getStatusCreationTime();
    auto statusCollectionTime = todStatusMessage->getCollectionTime();

    EV_INFO << "handleStatusUpdateMessage " << actorId << "," << statusId << endl;

    double lossRatio = computeLossRatio(todStatusMessage->getFrameId());

    auto instructionId = carlaCommunicationManager->computeInstruction(actorId, statusId, agentId, lossRatio);

    auto packet = new Packet("Instruction");
    auto data = makeShared<TodInstructionMessage>();

    // Data
    data->setActorId(actorId);
    data->setInstructionId(instructionId.c_str());
    data->setStatusCreationTime(statusCreationTime);
    data->setStatusDataCollectionTime(statusCollectionTime);
    data->setStatusProcessingTime(todStatusMessage->getTimestamp());
    data->setInstructionCreationTime(simTime());
    data->setLossRatio(lossRatio);

    auto creationTimeTag = data->addTag<CreationTimeTag>(); // add new tag
    creationTimeTag->setCreationTime(simTime()); // store current time

    packet->insertAtBack(data);

    // Reply on the QUIC connection that delivered the status (instruction on stream 0).
    auto it = replySocketByActor.find(actorId);
    if (it != replySocketByActor.end()) {
        sendPacket(it->second, packet, 0);
    } else {
        EV_WARN << "No reply socket for actor " << actorId << ", dropping instruction" << endl;
        delete packet;
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
    socket.listen();                // QUIC server: accept incoming connections
    socket.setCallback(this);
}


void TODAgentApp::handleStopOperation(LifecycleOperation *operation)
{
    socket.close();
    for (QuicSocket *cs : clientSockets) cs->close();
}

void TODAgentApp::handleCrashOperation(LifecycleOperation *operation)
{
    socket.setCallback(nullptr);
    for (QuicSocket *cs : clientSockets) cs->setCallback(nullptr);
}


void TODAgentApp::socketConnectionAvailable(QuicSocket *socket)
{
    // QUIC server: a new connection is available, accept it.
    QuicSocket *clientSocket = socket->accept();
    clientSocket->setCallback(this);
    clientSockets.push_back(clientSocket);
    EV_INFO << "TODAgentApp: accepted connection, client socket " << clientSocket->getSocketId() << endl;
}

// Retrieving data available from the socket
void TODAgentApp::socketDataAvailable(QuicSocket *socket, QuicDataInfo *dataInfo)
{
    socket->recv(dataInfo->getAvaliableDataSize(), dataInfo->getStreamID());
}

void TODAgentApp::socketDataArrived(QuicSocket *socket, Packet *packet)
{
    emit(packetReceivedSignal, packet);
    auto streamReq = packet->findTag<QuicStreamReq>();
    int streamID = streamReq ? streamReq->getStreamID() : 0;

    EV_INFO << "Received packet from stream: " << streamID << endl;

    processPacket(socket, packet, streamID);

    delete packet;
    numReceived++;
}


void TODAgentApp::socketClosed(QuicSocket *socket){}

void TODAgentApp::socketDestroyed(QuicSocket *socket)
{
    if (socket == &this->socket) return;

    for (std::vector<QuicSocket*>::iterator it = clientSockets.begin(); it != clientSockets.end(); ++it) if (*it == socket) { clientSockets.erase(it); break; }

    for (auto it = replySocketByActor.begin(); it != replySocketByActor.end(); ) {
        if (it->second == socket) it = replySocketByActor.erase(it);
        else ++it;
    }
    delete socket;
}

void TODAgentApp::sendPacket(QuicSocket *socket, Packet *packet, uint64_t streamId)
{
    emit(packetSentSignal, packet);
    socket->send(packet, streamId);
    numSent++;
}


void TODAgentApp::handleStatusUpdateMessage(QuicSocket *socket, Packet *statusPacket)
{
    auto todStatusMessage = statusPacket->peekData<TodStatusUpdateMessage>();
    auto actorId = todStatusMessage->getActorId();
    auto statusId = todStatusMessage->getStatusId();

    EV_INFO << "Received message for "<< actorId << " Status "<< statusId << endl;

    replySocketByActor[actorId] = socket;

    uint64_t frameId = todStatusMessage->getFrameId();
    frameAcc[frameId].expected = todStatusMessage->getExpectedSensorData();

    ProcessedStatusMessage *pkt = new ProcessedStatusMessage("processStatusMessage", PROCESS_STATUS_MESSAGE_KIND);
    pkt->setActorId(todStatusMessage->getActorId());
    pkt->setStatusId(todStatusMessage->getStatusId());
    pkt->setStatusCreationTime(statusPacket->peekData<TodStatusUpdateMessage>()->getAllTags<CreationTimeTag>()[0].getTag()->getCreationTime());
    pkt->setCollectionTime(todStatusMessage->getCollectionTime());
    pkt->setFrameId(frameId);
    pkt->setTimestamp();

    double processingStatusTime = par("processingStatusTime");
    scheduleAfter(processingStatusTime, pkt);
}


void TODAgentApp::processPacket(QuicSocket *socket, Packet *packet, int streamID)
{
    if (packet->hasData<TODMessage>()){
        if (packet->peekData<TODMessage>()->getMessageType() == TODMessageType::STATUS){
            EV_INFO << "Received status/control (stream " << streamID << ")" << endl;
            handleStatusUpdateMessage(socket, packet);
        } else EV_WARN << "Received an unexpected TOD Message " <<  packet->peekData<TODMessage>()->getMessageType()  << " check your implementation"<< endl;
    }
    else if (packet->hasData<SensorDataResponse>()) countSensorData(packet);
    else EV_WARN << "Received an unexpected packet "<< packet->getName() <<endl;
}


void TODAgentApp::countSensorData(Packet *packet)
{
    auto resp = packet->peekData<SensorDataResponse>();
    uint64_t frameId = resp->getFrameId();

    if (frameId <= lastClosedFrame) {
        EV_INFO << "Sensor data for frame " << frameId << " arrived after deadline (dropped)" << endl;
        return;
    }

    frameAcc[frameId].arrived++;
    EV_INFO << "Sensor data frame " << frameId << " stream " << resp->getStreamId()
            << " (" << resp->getSensorType() << "), arrived " << frameAcc[frameId].arrived << endl;
}


double TODAgentApp::computeLossRatio(uint64_t frameId)
{
    if (frameId > lastClosedFrame) lastClosedFrame = frameId;

    auto it = frameAcc.find(frameId);
    if (it == frameAcc.end()) return 0.0;

    int expected = it->second.expected;
    int arrived  = it->second.arrived;
    frameAcc.erase(it);

    if (expected <= 0) return 0.0;
    if (arrived > expected) arrived = expected;

    double loss = 1.0 - (double) arrived / (double) expected;
    EV_INFO << "Frame " << frameId << ": arrivati " << arrived << "/" << expected
            << " -> lossRatio " << loss << endl;
    return loss;
}
