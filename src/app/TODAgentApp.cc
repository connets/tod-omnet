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
using namespace std;

Define_Module(TODAgentApp);


void ProcessStatusTimeFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet*>(object);

    /*
     * Done to avoid chunk conversion error since the Agent handles
     *      - sensor datagram data
     *      - status update messages
     *      - TODInstructionMessage
     * during the simulation. Only TODInstructionMessage carries an
     * instruction message and with the other messages peekData throws
     * an error
     */
    if (strncmp(packet->getName(), "SensorDatagram", 14) != 0)
    {
        return;
    }

    if (strncmp(packet->getName(), "StatusUpdate", 12) != 0)
    {
        return;
    }

    simtime_t retrievalTime = packet->peekData<TodInstructionMessage>()->getStatusProcessingTime();

    auto instrucionDelay = simTime() - retrievalTime;

    fire(this, simTime(), instrucionDelay,  details);
}


TODAgentApp::~TODAgentApp()
{
    for (QuicSocket *cs : clientSockets)
    {
        delete cs;
    }
}

void TODAgentApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL)
    {
        agentId = par("agentId").stdstringValue();
        carlaCommunicationManager = check_and_cast<TodCarlanetManager*>(getParentModule()->getParentModule()->getSubmodule("carlaCommunicationManager"));
    }
}


void TODAgentApp::handleMessageWhenUp(cMessage* msg)
{
    if (msg->isSelfMessage())
    {
        if (msg->getKind() == PROCESS_STATUS_MESSAGE_KIND)
        {
            ProcessedStatusMessage *packet = dynamic_cast<ProcessedStatusMessage*>(msg);
            calcAndSendnstruction(packet);

            /*
             * Packet deletion to avoid leak during simulation
             */
            delete packet;
        }
    }
    else if (socket.belongsToSocket(msg))
    {
        socket.processMessage(msg);
    }
    else
    {
        /*
         * Handles various clients with their specific sockets
         */
        for (QuicSocket *clientSocket : clientSockets)
        {
            if (clientSocket->belongsToSocket(msg))
            {
                clientSocket->processMessage(msg);
                break;
            }
        }
    }
}

/*
 * This method calculate and send the instruction to the TODCarApp
 */
void TODAgentApp::calcAndSendnstruction(ProcessedStatusMessage *todStatusMessage)
{
    infoFromTodStatusMessage(todStatusMessage);
    EV_INFO << "handleStatusUpdateMessage " << currentInfos.actorId << "," << currentInfos.statusId << endl;

    double lossRatio = computeLossRatio(todStatusMessage->getFrameId());
    auto instructionId = carlaCommunicationManager->computeInstruction(currentInfos.actorId, currentInfos.statusId, agentId, lossRatio);

    createAndSendInstructionMessage(todStatusMessage, instructionId, lossRatio);
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
    socket.listen();                          // QUIC server: accept incoming connections
    socket.setCallback(this);
}


void TODAgentApp::handleStopOperation(LifecycleOperation *operation)
{
    socket.close();
    for (QuicSocket *clientSocket : clientSockets)
    {
        clientSocket->close();
    }
}

void TODAgentApp::handleCrashOperation(LifecycleOperation *operation)
{
    socket.setCallback(nullptr);
    for (QuicSocket *clientSocket : clientSockets)
    {
        clientSocket->setCallback(nullptr);
    }
}


void TODAgentApp::socketConnectionAvailable(QuicSocket *socket)
{
    QuicSocket *clientSocket = socket->accept();
    clientSocket->setCallback(this);
    clientSockets.push_back(clientSocket);
    EV_INFO << "TODAgentApp: accepted connection, client socket " << clientSocket->getSocketId() << endl;
}


void TODAgentApp::socketDataAvailable(QuicSocket *socket, QuicDataInfo *dataInfo)
{
    /*
     * The status update message is sent on stream 0 as fixed size messages.
     *
     * Under load of sensor datagrams the status update frame may be divided in
     * more packets since PacketBuilder occupies all the possible space with datagrams
     * and then the remaining space with the update status message.
     * This causes to read partial bytes and processPacket could throw conversion
     * errors.
     *
     * The solution is to read the message when there's one complete message
     * available using the fixed message length to fetch data from the socket.
     * The other bytes will be read in another moments and will remain bufferized
     *
     */
    const int64_t msgLen = par("statusMessageLength").intValue();
    if ((int64_t) dataInfo->getAvaliableDataSize() >= msgLen)
    {
        socket->recv(msgLen, dataInfo->getStreamID());
    }
}

void TODAgentApp::socketDataArrived(QuicSocket *socket, Packet *packet)
{
    emit(packetReceivedSignal, packet);
    auto streamReq = packet->findTag<QuicStreamReq>();
    int streamId = streamReq ? streamReq->getStreamID() : 0;

    EV_INFO << "Received packet from stream: " << streamId << endl;

    processPacket(socket, packet, streamId);

    /*
     * Packet deletion to avoid leaks
     */
    delete packet;
    numReceived++;
}

void TODAgentApp::socketDatagramArrived(QuicSocket *socket, Packet *packet)
{
    // Sensors data arrive as QUIC Datagram
    emit(packetReceivedSignal, packet);
    countSensorData(packet);
    delete packet;
    numReceived++;
}


void TODAgentApp::socketClosed(QuicSocket *socket) {}

void TODAgentApp::socketDestroyed(QuicSocket *socket)
{
    if (socket == &this->socket)
    {
        return;
    }

    // Clears the socket from the open sockets vector
    for (vector<QuicSocket*>::iterator socketIterator = clientSockets.begin(); socketIterator != clientSockets.end(); ++socketIterator)
    {
        if (*socketIterator == socket)
        {
            clientSockets.erase(socketIterator);
            break;
        }
    }

    // Clears the socket from the map actorId -> socket
    for (auto actorSocket = replySocketByActor.begin(); actorSocket != replySocketByActor.end(); )
    {
        if (actorSocket->second == socket)
        {
            actorSocket = replySocketByActor.erase(actorSocket);
        }
        else
        {
            ++actorSocket;
        }
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

    replySocketByActor[actorId] = socket; // Socket update for the given actor

    uint64_t frameId = todStatusMessage->getFrameId();
    auto& frameAcc = frameStats[frameId];

    /*
     * Flush of the old streams and setting up streams where data come
     * from in this frame
     */
    frameAcc.expectedStreams.clear();
    for (size_t i = 0; i < todStatusMessage->getExpectedStreamsArraySize(); i++)
    {
        frameAcc.expectedStreams.push_back(todStatusMessage->getExpectedStreams(i));
    }

    /*
     * Self message scheduling to sim the time of processing the status update message
     */
    scheduleStatusMessage(todStatusMessage, statusPacket, frameId);
}

/*
 * hasData and peekData throws conversion errors if the given chunk is not
 * of the correct type.
 * If on stream 0, for example, arrives a partial chunk with type SequenceChunk
 * or SliceChunk... the simulation could crash with conversion error.
 *
 * To avoid this dynamicPrtCast can be used: does not throw and converts
 * any message. With this the branch status is executed only when the message is
 * a TODMessage and also the peekData from the handleStatusUpdateMessage
 * cannot throw any kind of conversion error since TodStatusUpdateMessage is of type
 * TODMessage
 *
 * If its type is SensorDataReponse we execute the other branch
 */
void TODAgentApp::processPacket(QuicSocket *socket, Packet *packet, int streamID)
{
    auto dataChunk = packet->peekData<Chunk>();
    if (auto todMsg = dynamicPtrCast<const TODMessage>(dataChunk))
    {
        if (todMsg->getMessageType() == TODMessageType::STATUS)
        {
            EV_INFO << "Received status/control (stream " << streamID << ")" << endl;
            handleStatusUpdateMessage(socket, packet);
        }
        else
        {
            EV_WARN << "Received an unexpected TOD Message " << todMsg->getMessageType() << " check your implementation" << endl;
        }
    }
    else if (dynamicPtrCast<const SensorDataResponse>(dataChunk))
    {
        countSensorData(packet);
    }
    else
    {
        EV_WARN << "Received an unexpected packet" << endl;
    }
}

/*
 * This method counts all the sensor packet for a given frame and
 * sets them inside a stats struct each with its stream and total
 * fragments (written inside all of the packets)
 */
void TODAgentApp::countSensorData(Packet *packet)
{
    auto data = packet->peekAtFront<SensorDataResponse>();
    uint64_t frameId = data->getFrameId();

    /*
     * Drop of late frames
     */
    if (frameId <= lastClosedFrame)
    {
        EV_INFO << "Datagram sensor frame " << frameId << " arrived after deadline" << endl;
        return;
    }

    auto& stats = frameStats[frameId].statsPerStream[data->getStreamId()];
    stats.arrived++;
    stats.total = (int) data->getTotalFragments();
    EV_INFO << "Datagram sensor frame " << frameId << " stream " << data->getStreamId()
            << " fragment " << data->getFragmentNum() << "/" << data->getTotalFragments()
            << " (arrived " << stats.arrived << ")" << endl;
}

/*
 * This method computes the loss ratio weighted for
 * each stream (sensor).
 */
double TODAgentApp::computeLossRatio(uint64_t frameId)
{
    if (frameId > lastClosedFrame)
    {
        lastClosedFrame = frameId;
    }

    auto stats = frameStats.find(frameId);
    if (stats == frameStats.end())
    {
        return 0.0;
    }

    FrameAcc &frameAcc = stats->second;

    double sumWeighted = 0.0, sumWeightedLoss = 0.0;

    /*
     * Loop between all the EXPECTED streams
     */
    for (uint64_t streamId : frameAcc.expectedStreams)
    {
        double loss_s;
        auto streamStats = frameAcc.statsPerStream.find(streamId);

        /*
         * If the current stream has no stats or zero packets are arrived
         * the loss ratio is equal to 1 otherwise it's possibile to
         * compute it as follows
         */
        if (streamStats == frameAcc.statsPerStream.end() || streamStats->second.arrived == 0)
        {
            loss_s = 1.0;
        }
        else
        {
            int total = streamStats->second.total;
            int arrived = streamStats->second.arrived;

            if (total <= 0)
            {
                loss_s = 0.0; // Fallback case (when totalFragments is a negative number)
            }
            else
            {
                if (arrived > total)
                {
                    arrived = total;
                }

                loss_s = 1.0 - (double) arrived / (double) total;
            }
        }
        double weightPerSensor = 1.0; // With 1 all sensors have the same weight
        sumWeighted += weightPerSensor;
        sumWeightedLoss += weightPerSensor * loss_s;
    }

    /*
     * Normalize the loss between 0 and 1
     */
    double loss = (sumWeighted > 0.0) ? sumWeightedLoss / sumWeighted : 0.0;

    EV_INFO << "Frame " << frameId << ": weighted lossRatio " << loss
            << " (expected sensors " << frameAcc.expectedStreams.size() << ")" << endl;

    frameStats.erase(stats);
    return loss;
}

/*
 * Helper functions
 */
void createAndSendInstructionMessage(ProcessedStatusMessage *todStatusMessage, auto instructionId, double lossRatio)
{
    auto packet = new Packet("Instruction");
    auto data = makeShared<TodInstructionMessage>();

    data->setActorId(currentInfos.actorId);
    data->setInstructionId(instructionId.c_str());
    data->setStatusCreationTime(currentInfos.statusCreationTime);
    data->setStatusDataCollectionTime(currentInfos.statusCollectionTime);
    data->setStatusProcessingTime(todStatusMessage->getTimestamp());
    data->setInstructionCreationTime(simTime());
    data->setLossRatio(lossRatio);

    auto creationTimeTag = data->addTag<CreationTimeTag>();
    creationTimeTag->setCreationTime(simTime());

    /*
     * The length of the message is fixed and aligned so that no conversion
     * error can be thrown
     */
    data->setChunkLength(B(par("instructionMessageLength").intValue()));

    packet->insertAtBack(data);

    auto actorToReply = replySocketByActor.find(currentInfos.actorId);
    if (actorToReply != replySocketByActor.end())
    {
        sendPacket(actorToReply->second, packet, 0);
    }
    else
    {
        EV_WARN << "No reply socket for actor " << currentInfos.actorId << ", dropping instruction" << endl;
        delete packet;
    }
}

void scheduleStatusMessage(auto todStatusMessage, Packet *statusPacket, uint64_t frameId)
{
    ProcessedStatusMessage *packet = new ProcessedStatusMessage("processStatusMessage", PROCESS_STATUS_MESSAGE_KIND);
    packet->setActorId(todStatusMessage->getActorId());
    packet->setStatusId(todStatusMessage->getStatusId());
    packet->setStatusCreationTime(statusPacket->peekData<TodStatusUpdateMessage>()->getAllTags<CreationTimeTag>()[0].getTag()->getCreationTime());
    packet->setCollectionTime(todStatusMessage->getCollectionTime());
    packet->setFrameId(frameId);
    packet->setTimestamp();

    double processingStatusTime = par("processingStatusTime");
    scheduleAfter(processingStatusTime, packet);
}
