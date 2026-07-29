//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004,2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "TODAgentApp.h"

#include <cmath>
#include <algorithm>

#include "../sensors/messages/SensorMessages_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/TagBase_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/L3AddressResolver.h"

using namespace omnetpp;
using namespace inet;
using namespace std;

Define_Module(TODAgentApp);


/*
 * packetSent also fires on sensor datagrams that the agent does NOT emit,
 * so the source is really the outgoing instruction: guard by packet name so
 * peekData does not throw a conversion error
 */
void ProcessStatusTimeFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet*>(object);

    if (strncmp(packet->getName(), "Instruction", 11) != 0)
    {
        return;
    }

    auto instruction = packet->peekData<TodInstructionMessage>();
    auto processTime = simTime() - instruction->getStatusProcessingTime();

    fire(this, simTime(), processTime, details);
}


TODAgentApp::~TODAgentApp()
{
    for (auto& slot : actorSlots)
    {
        cancelAndDelete(slot.second.deadline);
    }

    for (QuicSocket *clientSocket : clientSockets)
    {
        delete clientSocket;
    }
}

void TODAgentApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL)
    {
        agentId = par("agentId").stdstringValue();
        samplingInterval = par("samplingInterval");
        deadlineSlack = par("deadlineSlack");
        carlaCommunicationManager = check_and_cast<TodCarlanetManager*>(getParentModule()->getParentModule()->getSubmodule("carlaCommunicationManager"));
    }
}

void TODAgentApp::refreshDisplay() const {}

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

/*
 * Self-messages are the two stages of the agent's own clock: the slot deadline,
 * which closes the collection window, and the delivery that follows it after the
 * modelled processing time.
 */
void TODAgentApp::handleMessageWhenUp(cMessage* msg)
{
    if (msg->isSelfMessage())
    {
        if (msg->getKind() == SLOT_DEADLINE_MSG_KIND)
        {
            /*
             * The deadline is periodic: closeSlot re-arms this very message, so it
             * must NOT be deleted here.
             */
            auto timer = check_and_cast<ProcessedStatusMessage*>(msg);
            closeSlot(timer->getActorId());
        }
        else if (msg->getKind() == SLOT_DELIVER_MSG_KIND)
        {
            auto timer = check_and_cast<ProcessedStatusMessage*>(msg);
            deliverSlot(timer->getActorId());
            delete timer;
        }
        else
        {
            delete msg;
        }
    }
    else if (socket.belongsToSocket(msg))
    {
        socket.processMessage(msg);
    }
    else
    {
        // Route to the right accepted client connection
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


void TODAgentApp::socketConnectionAvailable(QuicSocket *socket)
{
    QuicSocket *clientSocket = socket->accept();
    clientSocket->setCallback(this);
    clientSockets.push_back(clientSocket);
    EV_INFO << "TODAgentApp: accepted connection, client socket " << clientSocket->getSocketId() << endl;
}


/*
 * All application data arrives as QUIC DATAGRAM. Every datagram is a SensorData
 * fragment that self-describes its frame (statusId + expectedStreams), so the
 * frame is opened from the first fragment that survives (see countSensorData).
 */
void TODAgentApp::socketDatagramArrived(QuicSocket *socket, Packet *packet)
{
    emit(packetReceivedSignal, packet);

    if (packet->hasAtFront<SensorData>())
    {
        countSensorData(socket, packet);
    }
    else
    {
        EV_WARN << "TODAgentApp: unexpected datagram " << packet->getName() << endl;
    }

    delete packet;
    numReceived++;
}

void TODAgentApp::socketDataArrived(QuicSocket *socket, Packet *packet)
{
    delete packet;
}

void TODAgentApp::socketDataAvailable(QuicSocket *socket, QuicDataInfo *dataInfo) {}

void TODAgentApp::socketClosed(QuicSocket *socket) {}

void TODAgentApp::socketDestroyed(QuicSocket *socket)
{
    if (socket == &this->socket)
    {
        return;
    }

    // Remove the socket from the accepted-connections vector
    for (auto socketIterator = clientSockets.begin(); socketIterator != clientSockets.end(); ++socketIterator)
    {
        if (*socketIterator == socket)
        {
            clientSockets.erase(socketIterator);
            break;
        }
    }

    // Remove any actor->socket reply mapping pointing to it (avoid dangling use)
    // and stop the slot clock of the actors that were reached through it.
    for (auto socketIterator = replySocketByActor.begin(); socketIterator != replySocketByActor.end(); )
    {
        if (socketIterator->second == socket)
        {
            dropActor(socketIterator->first);
            socketIterator = replySocketByActor.erase(socketIterator);
        }
        else
        {
            ++socketIterator;
        }
    }

    delete socket;
}



/*
 * Starts the actor's clock on the first datagram ever seen from it, so loss is
 * not counted before the QUIC connection is actually carrying anything. The
 * first deadline lands one full period later: the slot that has just begun is
 * the first one we judge.
 */
void TODAgentApp::startSlotClock(const string& actorId)
{
    ActorSlot& slot = actorSlots[actorId];
    if (slot.deadline != nullptr)
    {
        return;
    }

    slot.deadline = new ProcessedStatusMessage("slotDeadline", SLOT_DEADLINE_MSG_KIND);
    slot.deadline->setActorId(actorId.c_str());
    slot.slotStart = simTime();
    scheduleAfter(samplingInterval + deadlineSlack, slot.deadline);

    EV_INFO << "TODAgentApp: slot clock started for actor " << actorId
            << ", period " << samplingInterval << " slack " << deadlineSlack << endl;
}

/*
 * Marks a statusId as accounted for, so any fragment of it arriving after the
 * deadline is recognised as late and dropped instead of reopening the frame.
 * The memory is deliberately bounded.
 */
void TODAgentApp::settle(ActorSlot& slot, const string& statusId)
{
    if (!slot.settled.insert(statusId).second)
    {
        return;
    }

    slot.settledOrder.push_back(statusId);
    while (slot.settledOrder.size() > SETTLED_HISTORY)
    {
        slot.settled.erase(slot.settledOrder.front());
        slot.settledOrder.pop_front();
    }
}

/*
 * Accumulates one sensor fragment into the slot that is currently open for its
 * actor. Nothing is decided here: the fragment either makes the deadline or it
 * does not, and the deadline is what decides.
 */
void TODAgentApp::countSensorData(QuicSocket *socket, Packet *packet)
{
    auto data = packet->peekAtFront<SensorData>();
    string actorId = data->getActorId();
    string statusId = data->getStatusId();

    replySocketByActor[actorId] = socket;
    startSlotClock(actorId);

    ActorSlot& slot = actorSlots[actorId];

    if (slot.settled.count(statusId))
    {
        EV_INFO << "TODAgentApp: late datagram for settled status " << statusId << endl;
        return;
    }

    auto entry = slot.open.find(statusId);
    if (entry == slot.open.end())
    {
        StatusAcc fresh;
        fresh.actorId = actorId;
        fresh.statusId = statusId;
        fresh.collectionTime = data->getCollectionTime();
        fresh.firstArrivalTime = simTime();
        fresh.qualityLevel = data->getQualityLevel();
        for (size_t i = 0; i < data->getExpectedStreamsArraySize(); i++)
        {
            fresh.expectedStreams.push_back(data->getExpectedStreams(i));
        }
        entry = slot.open.emplace(statusId, fresh).first;
    }
    else if (entry->second.expectedStreams.empty() && data->getExpectedStreamsArraySize() > 0)
    {
        // the fragment that carried the expected set may not be the first to land
        for (size_t i = 0; i < data->getExpectedStreamsArraySize(); i++)
        {
            entry->second.expectedStreams.push_back(data->getExpectedStreams(i));
        }
    }

    auto& stats = entry->second.statsPerStream[data->getStreamId()];
    stats.arrived++;
    stats.total = (int) data->getTotalFragments();

    EV_INFO << "TODAgentApp: status " << statusId << " stream " << data->getStreamId()
            << " fragment " << data->getFragmentNum() << "/" << data->getTotalFragments()
            << " (arrived " << stats.arrived << ")" << endl;
}

/*
 * The deadline fired: the window is over and we judge what is in it.
 *
 * A slot can legitimately hold fragments of more than one status when jitter
 * spreads a frame across the boundary. We pick the MOST COMPLETE one, freshest
 * first on a tie: picking the newest unconditionally would report catastrophic
 * loss whenever a frame has only just started arriving, which is a measurement
 * artefact rather than a real degradation.
 *
 * Everything seen in this window is settled either way, so nothing is counted
 * twice, and an empty window is left empty: 100% loss needs no special case.
 */
void TODAgentApp::closeSlot(const string& actorId)
{
    ActorSlot& slot = actorSlots[actorId];

    StatusAcc chosen;
    bool haveChoice = false;
    double bestCompleteness = -1.0;

    for (auto& entry : slot.open)
    {
        const StatusAcc& candidate = entry.second;

        int arrived = 0;
        int total = 0;
        for (const auto& stream : candidate.statsPerStream)
        {
            arrived += min(stream.second.arrived, stream.second.total);
            total += stream.second.total;
        }
        double completeness = (total > 0) ? (double) arrived / (double) total : 0.0;

        if (!haveChoice
            || completeness > bestCompleteness
            || (completeness == bestCompleteness && candidate.collectionTime > chosen.collectionTime))
        {
            chosen = candidate;
            bestCompleteness = completeness;
            haveChoice = true;
        }
    }

    for (const auto& entry : slot.open)
    {
        settle(slot, entry.first);
    }
    slot.open.clear();

    if (!haveChoice)
    {
        // Nothing made the deadline. Synthesise the placeholder the delivery step
        // needs, with the window as the reference instant.
        chosen = StatusAcc();
        chosen.actorId = actorId;
        chosen.collectionTime = slot.slotStart;
        chosen.firstArrivalTime = simTime();
    }

    slot.toDeliver.push_back(chosen);
    slot.slotStart = simTime();

    auto deliver = new ProcessedStatusMessage("slotDeliver", SLOT_DELIVER_MSG_KIND);
    deliver->setActorId(actorId.c_str());
    scheduleAfter(par("processingStatusTime"), deliver);

    scheduleAfter(samplingInterval, slot.deadline);
}

/*
 * Processing time is over, so the instruction goes out.
 *
 * With a status: ask CARLA to compute on it and forward the id. Without one:
 * still answer, with the no-instruction id and a loss of 1. That answer matters,
 * because it is how a car with a dead uplink and a healthy downlink learns that
 * the operator is seeing nothing at all. No CARLA round trip is made for it, so
 * no stale status is ever looked up.
 */
void TODAgentApp::deliverSlot(const string& actorId)
{
    auto slotEntry = actorSlots.find(actorId);
    if (slotEntry == actorSlots.end() || slotEntry->second.toDeliver.empty())
    {
        return;
    }

    ActorSlot& slot = slotEntry->second;
    StatusAcc status = slot.toDeliver.front();
    slot.toDeliver.pop_front();

    if (status.statusId.empty())
    {
        EV_INFO << "TODAgentApp: empty slot for actor " << actorId << ", reporting full loss" << endl;
        sendInstruction(status, NO_INSTRUCTION_ID, 1.0);
        return;
    }

    double lossRatio = computeLossRatio(status);
    EV_INFO << "TODAgentApp: slot for actor " << actorId << " on status " << status.statusId
            << " lossRatio " << lossRatio << " qualityLevel " << status.qualityLevel << endl;

    string instructionId = carlaCommunicationManager->computeInstruction(actorId, status.statusId, agentId,
                                                                        lossRatio, status.qualityLevel);
    sendInstruction(status, instructionId, lossRatio);
}

/*
 * Fraction of the expected sensor payload that did not make the deadline,
 * averaged over the expected streams.
 *
 * There is no reuse of older frames here on purpose. That coefficient only
 * existed because a reactive window had nothing to report when no frame opened;
 * with a fixed cadence every slot is accounted for, and a stale-data model
 * belongs on the CARLA side, where the actual old state still exists, rather
 * than in a decay constant that has to be justified.
 */
double TODAgentApp::computeLossRatio(const StatusAcc& status) const
{
    if (status.expectedStreams.empty())
    {
        return 0.0;
    }

    double sumLoss = 0.0;

    for (uint64_t streamId : status.expectedStreams)
    {
        double received = 0.0;

        auto stream = status.statsPerStream.find(streamId);
        if (stream != status.statsPerStream.end() && stream->second.total > 0)
        {
            int arrived = min(stream->second.arrived, stream->second.total);
            received = (double) arrived / (double) stream->second.total;
        }

        sumLoss += 1.0 - received;

        EV_INFO << "TODAgentApp:   stream " << streamId << " received " << received << endl;
    }

    return sumLoss / (double) status.expectedStreams.size();
}

/*
 * Builds the instruction and sends it back to the car as a QUIC DATAGRAM. The
 * message is a single fixed-size, byte-aligned chunk that fits in one datagram.
 * If the actor's connection is gone the instruction is simply dropped.
 */
void TODAgentApp::sendInstruction(const StatusAcc& status, const string& instructionId, double lossRatio)
{
    auto reply = replySocketByActor.find(status.actorId);
    if (reply == replySocketByActor.end() || reply->second == nullptr)
    {
        EV_WARN << "TODAgentApp: no reply socket for actor " << status.actorId << ", dropping instruction" << endl;
        return;
    }

    auto packet = new Packet("Instruction");
    auto data = makeShared<TodInstructionMessage>();

    data->setActorId(status.actorId.c_str());
    data->setInstructionId(instructionId.c_str());
    data->setStatusDataCollectionTime(status.collectionTime);
    data->setStatusCreationTime(status.collectionTime);
    data->setStatusProcessingTime(status.firstArrivalTime);
    data->setInstructionCreationTime(simTime());
    data->setLossRatio(lossRatio);

    auto creationTimeTag = data->addTag<CreationTimeTag>();
    creationTimeTag->setCreationTime(simTime());

    data->setChunkLength(B(par("instructionMessageLength").intValue()));
    packet->insertAtBack(data);

    emit(packetSentSignal, packet);
    reply->second->sendDatagram(packet);
    numSent++;
}

/*
 * The actor's connection is gone: stop its clock and forget its state, otherwise
 * the deadline would keep firing and reporting full loss for a car that is no
 * longer there.
 */
void TODAgentApp::dropActor(const string& actorId)
{
    auto slotEntry = actorSlots.find(actorId);
    if (slotEntry == actorSlots.end())
    {
        return;
    }

    cancelAndDelete(slotEntry->second.deadline);
    actorSlots.erase(slotEntry);
    EV_INFO << "TODAgentApp: slot clock stopped for actor " << actorId << endl;
}
