//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004,2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "TODCarApp.h"

#include <math.h>
#include <set>
#include <cstring>

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

simsignal_t TODCarApp::instructionRttEwmaSignal = cComponent::registerSignal("instructionRttEwma");
simsignal_t TODCarApp::qualityLevelSignal = cComponent::registerSignal("qualityLevel");

void StatusCreationTime::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet*>(object);

    /*
     * Stream 0 removed: the frame is now made only of SensorDatagram fragments.
     * packetSent fires on every fragment, so we peek the collection time on a
     * sensor datagram and fire only ONCE per frame, when the statusId changes
     * (all fragments of a frame are emitted back-to-back with the same id).
     */
    if (strncmp(packet->getName(), "SensorDatagram", 14) != 0)
    {
        return;
    }

    auto data = packet->peekAtFront<SensorData>();
    string statusId = data->getStatusId();
    if (statusId == lastFiredStatusId)
    {
        return;
    }
    lastFiredStatusId = statusId;

    auto instrucionDelay = simTime() - data->getCollectionTime();

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

        auto mobilityModule = omnetpp::check_and_cast<CarlaInetMobility*>(getContainingNode(this)->getSubmodule("mobility"));

        sensorBuffer.clear();
        carlaCommunicationManager = check_and_cast<TodCarlanetManager*>(
                getContainingNode(this)->getParentModule()->getSubmodule("carlaCommunicationManager"));

        updateStatusSelfMessage = new cMessage("UpdateStatus");
        statusUpdateInterval = par("statusUpdateInterval");
        EV_INFO << "****** => " << statusUpdateInterval << endl;

        rttEwmaAlpha = par("rttEwmaAlpha").doubleValue();
        qualitySilenceTimeout = par("qualitySilenceTimeout");
        qualityMaxLevel = par("qualityMaxLevel").intValue();
    }
}

/*
 * Keeps the EWMA of how stale the arriving instructions are, purely as a
 * statistic: this is the delay the teleoperated vehicle actually suffers. It no
 * longer selects the camera quality, because from here we only ever see the
 * instructions that made it back, which says nothing about what the uplink
 * dropped on the way up.
 */
void TODCarApp::trackInstructionRtt(simtime_t instructionRtt)
{
    /*
     * The first sample seeds the average: starting from zero would make the car
     * spend its first frames pretending the link is perfect.
     */
    if (!hasRttSample)
    {
        instructionRttEwma = instructionRtt;
        hasRttSample = true;
    }
    else
    {
        double smoothed = rttEwmaAlpha * instructionRtt.dbl()
                        + (1.0 - rttEwmaAlpha) * instructionRttEwma.dbl();
        instructionRttEwma = SimTime(smoothed);
    }
    emit(instructionRttEwmaSignal, instructionRttEwma.dbl());
}

/*
 * Applies the level the agent asked for. The car does not second-guess it: the
 * agent measured the loss, this side only turns the number into fewer bytes.
 */
void TODCarApp::applyRequestedQuality(int requestedLevel)
{
    lastInstructionArrival = simTime();

    if (requestedLevel < 0)
    {
        requestedLevel = 0;
    }

    if (requestedLevel != qualityLevel)
    {
        EV_INFO << "TODCarApp: agent requests camera quality level "
                << qualityLevel << " => " << requestedLevel << endl;
        qualityLevel = requestedLevel;
    }

    emit(qualityLevelSignal, (long) qualityLevel);
}

/*
 * Fallback for a dead downlink.
 *
 * The agent decides the level, but its request rides on the instruction: if the
 * downlink is gone, no request can arrive and the level would freeze while the
 * camera keeps pushing the same bytes into a link that delivers nothing. Here the
 * car gives up one rung per silent timeout on its own.
 *
 * Only ever degrades: recovering needs a real request to come back. This is the
 * same idea as the dead-man's switch on the CARLA side, not a second mechanism -
 * that one stops the vehicle, this one shrinks the frame.
 */
void TODCarApp::degradeOnSilence()
{
    if (qualitySilenceTimeout <= SIMTIME_ZERO || !hasRttSample)
    {
        return;
    }

    simtime_t silence = simTime() - lastInstructionArrival;
    if (silence < qualitySilenceTimeout)
    {
        return;
    }

    int level = (int) (silence / qualitySilenceTimeout);
    if (level > qualityMaxLevel)
    {
        level = qualityMaxLevel;
    }

    if (level > qualityLevel)
    {
        EV_INFO << "TODCarApp: no instruction for " << silence
                << " -> camera quality level " << qualityLevel << " => " << level << endl;
        qualityLevel = level;
        emit(qualityLevelSignal, (long) qualityLevel);
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
 */
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
            /*
             * This branch executes when it's time to fetch all the data to build the status
             * update message. It collects sensor data via sending a message to the manager
             * and schedules a new self message to do again the status update
             */
            retrieveStatusData();

            /*
             * The poll carries the current quality level, so the sensors size the
             * next frame according to how the link is behaving right now. Check for
             * silence first: if nothing is coming back the level has to keep falling.
             */
            degradeOnSilence();

            auto collect = new SensorCollectRequest("collectSensors");
            collect->setQualityLevel(qualityLevel);
            send(collect, "toManager");

            scheduleAfter(statusUpdateInterval, msg);
        }
        else if (msg->getKind() == CREATION_STATUS_DATA_MSG_KIND)
        {
            sendUpdateStatusPacket(simTime());

            /*
             * Avoid message leaks during simulation as in this branch it will not used again
             */
            delete msg;
        }
    }
    else if (msg->arrivedOn("fromManager"))
    {
        /*
         * This branch captures all the message coming from the sensor manager.
         * Calling bufferizeSensorData, the sensors messages are stored inside a vector
         * ready to be used
         */
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


/*
 * Sends the whole frame to the agent using ONLY the QUIC DATAGRAM protocol:
 * one or more SensorData fragments per sampled sensor. Every fragment
 * self-describes the frame (statusId + expectedStreams), so the agent can open
 * the frame from the first fragment that survives. Everything is correlated by
 * statusId (the opaque CARLA status id).
 */
void TODCarApp::sendUpdateStatusPacket(simtime_t dataRetrievalTime)
{
    zeroDelay = par("zeroDelay").boolValue();
    EV_INFO << "TODCarApp::sendUpdateStatusPacket setting zero delay to => " << zeroDelay << endl;

    auto mobilityModule = check_and_cast<CarlaInetMobility*>(getContainingNode(this)->getSubmodule("mobility"));
    string carlaId = mobilityModule->getCarlaId();

    EV_INFO << "TODCarApp::sendUpdateStatusPacket zeroDelay "<< zeroDelay << endl;

    if (zeroDelay)
    {
        applyZeroDelay();
        return;
    }

    string statusId = carlaCommunicationManager->getActorStatus(carlaId);

    if (sensorBuffer.empty())
    {
        EV_INFO << "TODCarApp: empty sensor buffer for status " << statusId << ", skipping frame" << endl;
        return;
    }

    streamsThisFrame.clear();
    for (auto packet : sensorBuffer)
    {
        streamsThisFrame.insert(packet->peekAtFront<SensorDataResponse>()->getStreamId());
    }

    for(; not sensorBuffer.empty(); sensorBuffer.pop_back())
    {
        sendSensorPacket(sensorBuffer.back(), statusId, carlaId);
    }
}

/*
 * The instruction from the agent travels as a QUIC DATAGRAM.
 * It is a single fixed-size TodInstructionMessage that always fits in one datagram,
 * so no reassembly/framing is needed.
 */
void TODCarApp::socketDatagramArrived(QuicSocket *socket, Packet *packet)
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

void TODCarApp::socketDataArrived(QuicSocket *socket, Packet *packet)
{
    delete packet;
}

void TODCarApp::socketDataAvailable(QuicSocket *socket, QuicDataInfo *dataInfo) {}

void TODCarApp::socketClosed(QuicSocket *socket) {}

/*
 * This method send to the Agent all the sensor packet using the QUIC
 * Datagram protocol
 */
void TODCarApp::sendSensorPacket(Packet *packet, string statusId, string carlaId)
{
    // Contains all the info of the sensor
    auto source = packet->peekAtFront<SensorDataResponse>();
    infoFromSource(source, statusId, carlaId);

    int64_t dataBytes = B(packet->getByteLength()).get() - currentSource.headerBytes;
    if (dataBytes < 0)
    {
        dataBytes = 0;
    }

    int64_t chunkSize = par("datagramChunkSize").intValue();
    if (chunkSize <= 0)
    {
        chunkSize = 1000;
    }

    int totalFragments = fragmentNumber(dataBytes, chunkSize);

    createAndSendFragmentPacket(totalFragments, dataBytes, chunkSize);

    delete packet;
}

void TODCarApp::bufferizeSensorData(cMessage* msg)
{
    Packet* pkt = check_and_cast<Packet*>(msg);
    sensorBuffer.push_back(pkt);
}

void TODCarApp::processPacket(Packet *packet)
{
    if (packet->hasData<TODMessage>())
    {
        //TODO: COOP MESSAGE
        if (packet->peekData<TODMessage>()->getMessageType() == TODMessageType::INSTRUCTION)
        {
            auto message = packet->peekData<TodInstructionMessage>();

            /*
             * How stale this instruction is: the time between the sampling of the
             * frame that produced it and its arrival here. Statistic only.
             *
             * A no-instruction reply carries no real frame, so its collection time
             * is the agent's empty window and would poison the average.
             */
            if (strcmp(message->getInstructionId(), NO_INSTRUCTION_ID) != 0)
            {
                trackInstructionRtt(simTime() - message->getStatusDataCollectionTime());
            }

            /*
             * The quality request rides on every reply, including the empty-slot
             * one: that is exactly the case where the car most needs to be told to
             * shrink the frame, since from its own side the link looks quiet.
             */
            applyRequestedQuality(message->getRequestedQualityLevel());

            carlaCommunicationManager->applyInstruction(message->getActorId(), message->getInstructionId());
        }
        else
        {
            EV_WARN << "Received an unexpected TOD Message " <<  packet->peekData<TODMessage>()->getMessageType()  << " check your implementation"<< endl;
        }
    }
    else
    {
        EV_WARN << "Received an unexpected packet "<< packet->getName() <<endl;
    }
}

/*
 * Helper Functions
 */
void TODCarApp::applyZeroDelay()
{
    auto mobilityModule = check_and_cast<CarlaInetMobility*>(getContainingNode(this)->getSubmodule("mobility"));
    string carlaID = mobilityModule->getCarlaId();
    carlaCommunicationManager->getActorStatusZeroDelay(carlaID);
    for (auto packet : sensorBuffer)
    {
        delete packet;
    }

    sensorBuffer.clear();
}

/*
 * Fills the expectedStreams[] array of a network chunk with the set of streams
 * sampled in this frame. streamsThisFrame is computed ONCE per frame in
 * sendUpdateStatusPacket(), before the send loop drains sensorBuffer.
 */
template <typename ChunkPtr>
static void fillExpectedStreams(ChunkPtr &data, const set<uint64_t> &streams)
{
    data->setExpectedStreamsArraySize(streams.size());
    int idx = 0;
    for (uint64_t stream : streams)
    {
        data->setExpectedStreams(idx++, stream);
    }
}

void TODCarApp::createAndSendFragmentPacket(int totalFragments, int64_t dataBytes, int64_t chunkSize)
{
    for (int fragment = 0; fragment < totalFragments; fragment++)
    {
        auto newFragmentPacket = new Packet("SensorDatagram");
        auto data = makeShared<SensorData>();

        data->setStatusId(currentSource.statusId.c_str());
        data->setActorId(currentSource.carlaId.c_str());
        data->setStreamId(currentSource.streamId);
        data->setFragmentNum(fragment);
        data->setTotalFragments(totalFragments);
        data->setSensorType(currentSource.sensorType.c_str());
        data->setCollectionTime(currentSource.collectionTime);
        // travels to the agent, which reports it to CARLA together with the loss
        data->setQualityLevel(currentSource.qualityLevel);

        // Expected stream set is the same for every fragment of the frame
        fillExpectedStreams(data, streamsThisFrame);

        data->setChunkLength(B(currentSource.headerBytes));
        newFragmentPacket->insertAtBack(data);

        int64_t remaining = dataBytes - (int64_t) fragment * chunkSize;
        int64_t thisChunk = remaining < chunkSize ? remaining : chunkSize;
        if (thisChunk > 0)
        {
            newFragmentPacket->insertAtBack(makeShared<ByteCountChunk>(B(thisChunk)));
        }

        EV_INFO << "TODCarApp: sensor datagram on stream " << currentSource.streamId
                << ", status " << currentSource.statusId << ", fragment "
                << fragment << "/" << totalFragments << endl;
        emit(packetSentSignal, newFragmentPacket);
        socket.sendDatagram(newFragmentPacket);
    }
}
