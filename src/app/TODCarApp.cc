//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004,2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "TODCarApp.h"

#include <math.h>
#include <set>

#include "inet/common/ModuleAccess.h"
#include "inet/common/TagBase_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/quic/Quic.h"
#include "inet/transportlayer/contract/quic/QuicCommand_m.h"
#include "messages/TodMessages_m.h"
#include "../sensors/messages/SensorMessages_m.h"

using namespace omnetpp;
using namespace inet;
using namespace std;

Define_Module(TODCarApp);

void StatusCreationTime::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet*>(object);
    simtime_t retrievalTime = packet->peekData<TodStatusUpdateMessage>()->getCollectionTime();

    auto instrucionDelay = simTime() - retrievalTime;

    fire(this, simTime(), instrucionDelay,  details);
}


void instructionRTTNetworkFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet*>(object);
    auto instructionMessage = packet->peekData<TodInstructionMessage>();
    auto uplinkRtt = instructionMessage->getStatusProcessingTime() - instructionMessage->getStatusCreationTime();
    auto downLinkRtt = simTime() - instructionMessage->getInstructionCreationTime();

    auto rtt = downLinkRtt + uplinkRtt;

    fire(this, simTime(), rtt,  details);
}

void InstructionDelayResultFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
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
    if (stage == INITSTAGE_LOCAL)
    {
        zeroDelay = par("zeroDelay").boolValue();
        EV_INFO << "setting zero delay to => " << zeroDelay << endl;

        auto mobilityModule = omnetpp::check_and_cast<CarlaInetMobility*>(getParentModule()->getSubmodule("mobility"));

        sensorBuffer.clear();
        carlaCommunicationManager = check_and_cast<TodCarlanetManager*>(
                getParentModule()->getParentModule()->getSubmodule("carlaCommunicationManager"));

        updateStatusSelfMessage = new cMessage("UpdateStatus");
        statusUpdateInterval = par("statusUpdateInterval");
        EV_INFO << "****** => " << statusUpdateInterval << endl;
    }
}

void TODCarApp::refreshDisplay() const{}

void TODCarApp::finish()
{
    ApplicationBase::finish();
}

/*
 * The periodic status/sensor frame is started in socketEstablished(),
 * once the QUIC connection is ready
 * */
void TODCarApp::handleStartOperation(LifecycleOperation *operation)
{
    L3AddressResolver().tryResolve(par("destAddress"), destAddress);
    destPort = par("destPort");

    std::cout << "TODCarApp::handleStartOperation "<< destAddress << ":" << destPort <<  endl;

    socket.setOutputGate(gate("socketOut"));
    socket.bind(L3Address(), destPort);     // local bind (unspecified address)
    socket.connect(destAddress, destPort);  // start QUIC connection setup
    socket.setCallback(this);
}


void TODCarApp::handleStopOperation(LifecycleOperation *operation)
{
    socket.close();
}

void TODCarApp::handleCrashOperation(LifecycleOperation *operation)
{
    if (operation->getRootModule() != getContainingNode(this))
    {
        socket.destroy();
    }

    socket.setCallback(nullptr);
}


void TODCarApp::handleMessageWhenUp(cMessage* msg)
{
    if (msg->isSelfMessage())
    {
        if (msg == updateStatusSelfMessage)
        {
            retrieveStatusData();
            send(new cMessage("collectSensors"), "toManager");
            scheduleAfter(statusUpdateInterval, msg);
        }
        else if (msg->getKind() == CREATION_STATUS_DATA_MSG_KIND)
        {
            sendUpdateStatusPacket(simTime());
        }
    }
    else if (msg->arrivedOn("fromManager"))
    {
        bufferizeSensorData(msg);
    }
    else if(socket.belongsToSocket(msg))
    {
        socket.processMessage(msg);
    }
}



void TODCarApp::retrieveStatusData()
{
    double encTime = par("encodingImageTime");
    double collectTime = par("collectionDataTime");
    double creationStatusTime = encTime + collectTime;

    cMessage* msg = new cMessage("creationStatusTime", CREATION_STATUS_DATA_MSG_KIND);
    scheduleAfter(creationStatusTime, msg);
}



void TODCarApp::sendUpdateStatusPacket(simtime_t dataRetrievalTime)
{
    zeroDelay = par("zeroDelay").boolValue();
    EV_INFO << "TODCarApp::sendUpdateStatusPacket setting zero delay to => " << zeroDelay << endl;

    auto mobilityModule = check_and_cast<CarlaInetMobility*>(getParentModule()->getSubmodule("mobility"));
    string carlaID = mobilityModule->getCarlaId();

    EV_INFO << "TODCarApp::sendUpdateStatusPacket zeroDelay "<< zeroDelay << endl;

    if (zeroDelay)
    {
        applyZeroDelay();
        return;
    }

    string statusId = carlaCommunicationManager->getActorStatus(carlaID);
    uint64_t frameId = ++frameCounter;

    createAndSendStatusUpdateMessage(dataRetrievalTime, statusId, frameId, carlaID);

    for(; not sensorBuffer.empty(); sensorBuffer.pop_back())
    {
        sendSensorPacket(sensorBuffer.back(), frameId);
    }
}

void TODCarApp::socketDataArrived(QuicSocket *socket, Packet *packet)
{
    emit(packetReceivedSignal, packet);
    processPacket(packet);
    delete packet;
}


void TODCarApp::socketEstablished(QuicSocket *socket)
{
    EV_INFO << "TODCarApp: QUIC connection established, starting status loop" << endl;
    scheduleAt(simTime() + statusUpdateInterval, updateStatusSelfMessage);
}

void TODCarApp::socketDataAvailable(QuicSocket *socket, QuicDataInfo *dataInfo)
{
    socket->recv(dataInfo->getAvaliableDataSize(), dataInfo->getStreamID());
}

void TODCarApp::socketClosed(QuicSocket *socket) {}

void TODCarApp::sendSensorPacket(Packet *packet, uint64_t frameId)
{
    auto source = packet->peekAtFront<SensorDataResponse>();
    uint64_t streamId = source->getStreamId();
    string sensorType = source->getSensorType();
    simtime_t collectionTime = source->getCollectionTime();
    int64_t headerBytes = B(source->getChunkLength()).get();

    int64_t dataBytes = B(packet->getByteLength()).get() - headerBytes;
    if (dataBytes < 0)
    {
        dataBytes = 0;
    }

    int64_t chunkSize = par("datagramChunkSize").intValue();
    if (chunkSize <= 0)
    {
        chunkSize = 1000;
    }

    int totalFragments = (dataBytes <= 0) ? 1 : (int) ((dataBytes + chunkSize - 1) / chunkSize);

    for (int fragment = 0; fragment < totalFragments; fragment++)
    {
        auto newFragmentPacket = new Packet("SensorDatagram");
        auto data = makeShared<SensorDataResponse>();

        data->setStreamId(streamId);
        data->setFrameId(frameId);
        data->setFragmentNum(fragment);
        data->setTotalFragments(totalFragments);
        data->setSensorType(sensorType.c_str());
        data->setCollectionTime(collectionTime);
        data->setChunkLength(B(headerBytes));
        newFragmentPacket->insertAtBack(data);

        int64_t remaining = dataBytes - (int64_t) fragment * chunkSize;
        int64_t thisChunk = remaining < chunkSize ? remaining : chunkSize;
        if (thisChunk > 0)
        {
            newFragmentPacket->insertAtBack(makeShared<ByteCountChunk>(B(thisChunk)));
        }

        EV_INFO << "TODCarApp: datagram del sensore su stream " << streamId << ", per il frame " << frameId << " e frammento " << fragment << "/" << totalFragments << endl;
        emit(packetSentSignal, newFragmentPacket);
        socket.sendDatagram(newFragmentPacket);
    }

    delete packet;
}

void TODCarApp::bufferizeSensorData(cMessage* msg)
{
    Packet* pkt = check_and_cast<Packet*>(msg);
    sensorBuffer.push_back(pkt);
}

void TODCarApp::sendUpdatePacket(Packet *packet)
{
    emit(packetSentSignal, packet);
    socket.send(packet, 0);
}


// implementazione di diversi teleoperatori?
void TODCarApp::processPacket(Packet *pk)
{
    if (pk->hasData<TODMessage>())
    {
        if (pk->peekData<TODMessage>()->getMessageType() == TODMessageType::INSTRUCTION)
        {
            auto message = pk->peekData<TodInstructionMessage>();
            carlaCommunicationManager->applyInstruction(message->getActorId(), message->getInstructionId());
        }
        //TODO: COOP MESSAGE
        else
        {
            EV_WARN << "Received an unexpected TOD Message " <<  pk->peekData<TODMessage>()->getMessageType()  << " check your implementation"<< endl;
        }
    }
    else
    {
        EV_WARN << "Received an unexpected packet "<< pk->getName() <<endl;
    }
}

/*
 * Helper Functions
 */
void TODCarApp::applyZeroDelay()
{
    auto mobilityModule = check_and_cast<CarlaInetMobility*>(getParentModule()->getSubmodule("mobility"));
    std::string carlaID = mobilityModule->getCarlaId();
    carlaCommunicationManager->getActorStatusZeroDelay(carlaID);
    for (auto pk : sensorBuffer)
    {
        delete pk;
    }

    sensorBuffer.clear();
}

void TODCarApp::createAndSendStatusUpdateMessage(simtime_t dataRetrievalTime, string statusId, uint64_t frameId, string carlaID)
{
    L3AddressResolver().tryResolve(par("destAddress"), destAddress);
    EV_INFO << "Send status update for id: "<< carlaID << " to: "<< destAddress<<":"<<destPort<< endl;

    int statusMessageLength = par("statusMessageLength").intValue();
    EV_INFO << "Send status update message" << endl;

    auto packet = new Packet((string("StatusUpdate_")+statusId).c_str());
    auto data = makeShared<TodStatusUpdateMessage>();

    data->setActorId(carlaID.c_str());
    data->setStatusId(statusId.c_str());
    data->setCollectionTime(dataRetrievalTime);
    data->setFrameId(frameId);

    streamsThisFrame.clear(); // Flush of previous streams
    for (auto packet : sensorBuffer)
    {
        streamsThisFrame.insert(packet->peekAtFront<SensorDataResponse>()->getStreamId());
    }

    data->setExpectedStreamsArraySize(streamsThisFrame.size());
    int idx = 0;
    for (uint64_t stream : streamsThisFrame)
    {
        data->setExpectedStreams(idx++, stream);
    }

    auto creationTimeTag = data->addTag<CreationTimeTag>();
    creationTimeTag->setCreationTime(simTime());

    packet->insertAtBack(data);
    auto streamReq = packet->addTag<QuicStreamReq>();
    streamReq->setStreamID(0);

    sendUpdatePacket(packet);
}
