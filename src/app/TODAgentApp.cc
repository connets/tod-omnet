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
    for (auto& openFrame : openFrames)
    {
        cancelAndDelete(openFrame.second.closeTimer);
    }

    for (auto& watch : actorWatch)
    {
        cancelAndDelete(watch.second.timer);
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
        reuseDecay = par("reuseDecay").doubleValue();
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
 * Self-messages are only our per-frame close timers: when one fires the
 * corresponding frame has had enough time to accumulate its datagrams, so we
 * compute the loss and reply with the instruction.
 */
void TODAgentApp::handleMessageWhenUp(cMessage* msg)
{
    if (msg->isSelfMessage())
    {
        if (msg->getKind() == CLOSE_FRAME_MSG_KIND)
        {
            auto timer = check_and_cast<ProcessedStatusMessage*>(msg);
            closeFrame(timer->getStatusId());
            delete timer;
        }
        else if (msg->getKind() == WATCHDOG_MSG_KIND)
        {
            /*
             * The watchdog message is re-armed (or stopped) inside watchdogTick,
             * so it must NOT be deleted here.
             */
            auto timer = check_and_cast<ProcessedStatusMessage*>(msg);
            watchdogTick(timer->getActorId());
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
    for (auto socketIterator = replySocketByActor.begin(); socketIterator != replySocketByActor.end(); )
    {
        if (socketIterator->second == socket)
        {
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
 * Opens a frame on the first fragment seen for a statusId and arms the close
 * timer. If the frame is already open we only adopt the expected-stream set
 * when we didn't have it yet. Closed frames are ignored so late datagrams
 * cannot reopen them.
 */
void TODAgentApp::openFrame(const string& statusId, const string& actorId,
                            const vector<uint64_t>& expectedStreams, simtime_t collectionTime,
                            QuicSocket* replySocket)
{
    replySocketByActor[actorId] = replySocket;

    /*
     * The frame is already opened
     */
    if (closedFrames.count(statusId))
    {
        return;
    }

    /*
     * If the frame is already opened then we check for its expected streams:
     * if empty we assign it with the new one not empty
     */
    auto currentOpenFrame = openFrames.find(statusId);
    if (currentOpenFrame != openFrames.end())
    {
        if (currentOpenFrame->second.expectedStreams.empty() && !expectedStreams.empty())
        {
            currentOpenFrame->second.expectedStreams = expectedStreams;
        }

        return;
    }

    FrameAcc frame;
    frame.actorId = actorId;
    frame.expectedStreams = expectedStreams;
    frame.collectionTime = collectionTime;
    frame.firstArrivalTime = simTime();

    auto timer = new ProcessedStatusMessage("closeFrame", CLOSE_FRAME_MSG_KIND);
    timer->setStatusId(statusId.c_str());
    timer->setActorId(actorId.c_str());
    frame.closeTimer = timer;

    openFrames[statusId] = frame;
    scheduleAfter(par("processingStatusTime"), timer);

    /*
     * Register the activity and start the per-actor 100%-loss watchdog (once).
     *
     * This keeps track of the 100% loss even if no pieces of datagram arrives
     */
    ActorWatch& watch = actorWatch[actorId];
    watch.lastFrameOpenTime = simTime();
    if (watch.timer == nullptr)
    {
        watch.timer = new ProcessedStatusMessage("frameWatchdog", WATCHDOG_MSG_KIND);
        watch.timer->setActorId(actorId.c_str());
        scheduleAfter(par("frameInterval"), watch.timer);
    }

    EV_INFO << "TODAgentApp: opened frame " << statusId << " for actor " << actorId
            << " (" << expectedStreams.size() << " expected streams)" << endl;
}

void TODAgentApp::countSensorData(QuicSocket *socket, Packet *packet)
{
    auto data = packet->peekAtFront<SensorData>();
    string statusId = data->getStatusId();

    /*
     * Frame already closed: drop of any fragments with the given statusId
     */
    if (closedFrames.count(statusId))
    {
        EV_INFO << "TODAgentApp: late datagram for closed frame " << statusId << endl;
        return;
    }

    /*
     * If frame is already opened we build the expectedStreams array and then
     * we call the method to open the current frame to wait for new
     * fragments
     */
    if (!openFrames.count(statusId))
    {
        vector<uint64_t> expected;
        for (size_t i = 0; i < data->getExpectedStreamsArraySize(); i++)
        {
            expected.push_back(data->getExpectedStreams(i));
        }

        openFrame(statusId, data->getActorId(), expected, data->getCollectionTime(), socket);
    }

    auto& frame = openFrames[statusId];
    auto& stats = frame.statsPerStream[data->getStreamId()];
    stats.arrived++;
    stats.total = (int) data->getTotalFragments();

    EV_INFO << "TODAgentApp: frame " << statusId << " stream " << data->getStreamId()
            << " fragment " << data->getFragmentNum() << "/" << data->getTotalFragments()
            << " (arrived " << stats.arrived << ")" << endl;
}


/*
 * Closes a frame: computes the (reuse-aware) loss ratio, asks CARLA for the
 * instruction and sends it back via datagram. The statusId is then marked
 * closed so any straggler datagram is dropped.
 */
void TODAgentApp::closeFrame(const string& statusId)
{
    auto openFrame = openFrames.find(statusId);
    if (openFrame == openFrames.end())
    {
        return;
    }

    FrameAcc& frame = openFrame->second;

    double lossRatio = computeLossRatio(frame, frame.actorId);
    EV_INFO << "TODAgentApp: closing frame " << statusId << " lossRatio " << lossRatio << endl;

    string instructionId = carlaCommunicationManager->computeInstruction(frame.actorId, statusId, agentId, lossRatio);
    createAndSendInstructionMessage(frame, statusId, instructionId, lossRatio);

    ActorWatch& watch = actorWatch[frame.actorId];
    watch.lastStatusId = statusId;
    watch.lastExpectedStreams = frame.expectedStreams;

    closedFrames.insert(statusId);
    openFrames.erase(openFrame);
}

double TODAgentApp::computeLossRatio(FrameAcc& frame, const string& actorId)
{
    /*
     * Nothing was sampled from the sensors, so no loss to report
     */
    if (frame.expectedStreams.empty())
    {
        return 0.0;
    }

    auto& history = streamHistory[actorId];

    double sumLoss = 0.0;
    int streamCounts = 0;

    for (uint64_t streamId : frame.expectedStreams)
    {
        StreamHistory& streamState = history[streamId];
        streamState.expectCount++;

        /*
         * Fraction actually received this frame
         */
        double current = 0.0;
        auto streamStats = frame.statsPerStream.find(streamId);
        if (streamStats != frame.statsPerStream.end() && streamStats->second.total > 0)
        {
            int arrived = min(streamStats->second.arrived, streamStats->second.total);
            current = (double) arrived / (double) streamStats->second.total;
        }

        /*
         * Fraction still reusable from the past, decayed by how many consecutive
         * expected frames this sensor has missed since it last delivered
         */
        double reuse = 0.0;
        if (streamState.hasHistory)
        {
            uint64_t age = streamState.expectCount - streamState.lastGoodExpectCount;
            reuse = pow(reuseDecay, (double) age) * streamState.lastGoodRatio;
        }

        /*
         * Best choice, reuse or current
         */
        double effective = max(current, reuse);
        if (effective > 1.0)
        {
            effective = 1.0;
        }

        double loss_s = 1.0 - effective;

        sumLoss += loss_s;
        streamCounts++;

        if (current > 0.0 && current >= reuse)
        {
            streamState.lastGoodRatio = current;
            streamState.lastGoodExpectCount = streamState.expectCount;
            streamState.hasHistory = true;
        }

        EV_INFO << "TODAgentApp:   stream " << streamId << " cur " << current
                << " reuse " << reuse << " -> loss " << loss_s << endl;
    }

    double loss = (streamCounts > 0) ? sumLoss / (double) streamCounts : 0.0;
    EV_INFO << "TODAgentApp: frame lossRatio " << loss
            << " (expected sensors " << frame.expectedStreams.size() << ")" << endl;
    return loss;
}


/*
 * Builds the instruction and sends it back to the car as a QUIC DATAGRAM. The
 * message is a single fixed-size, byte-aligned chunk that fits in one datagram.
 * If the actor's connection is gone the instruction is simply dropped.
 */
void TODAgentApp::createAndSendInstructionMessage(FrameAcc& frame, const string& statusId,
                                                  const string& instructionId, double lossRatio)
{
    auto reply = replySocketByActor.find(frame.actorId);
    if (reply == replySocketByActor.end() || reply->second == nullptr)
    {
        EV_WARN << "TODAgentApp: no reply socket for actor " << frame.actorId << ", dropping instruction" << endl;
        return;
    }

    auto packet = new Packet("Instruction");
    auto data = makeShared<TodInstructionMessage>();

    data->setActorId(frame.actorId.c_str());
    data->setInstructionId(instructionId.c_str());
    data->setStatusDataCollectionTime(frame.collectionTime);
    data->setStatusCreationTime(frame.collectionTime);
    data->setStatusProcessingTime(frame.firstArrivalTime);
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

void TODAgentApp::watchdogTick(const string& actorId)
{
    auto watcher = actorWatch.find(actorId);
    if (watcher == actorWatch.end())
    {
        return;
    }

    ActorWatch& watch = watcher->second;

    /*
     * Stop the watchdog if the actor's connection is gone
     */
    if (replySocketByActor.find(actorId) == replySocketByActor.end())
    {
        cancelAndDelete(watch.timer);
        actorWatch.erase(watcher);
        return;
    }

    simtime_t frameInterval = par("frameInterval");

    while (!watch.lastStatusId.empty() &&
           (simTime() - watch.lastFrameOpenTime) >= frameInterval * 2)
    {
        watch.lastFrameOpenTime += frameInterval;

        FrameAcc lost;
        lost.actorId = actorId;
        lost.expectedStreams = watch.lastExpectedStreams;
        lost.collectionTime = watch.lastFrameOpenTime;
        lost.firstArrivalTime = simTime();

        double lossRatio = computeLossRatio(lost, actorId);

        EV_INFO << "TODAgentApp: 100% loss slot for actor " << actorId
                << " lossRatio " << lossRatio << " (reusing status " << watch.lastStatusId << ")" << endl;

        string instructionId = carlaCommunicationManager->computeInstruction(actorId, watch.lastStatusId, agentId, lossRatio);
        createAndSendInstructionMessage(lost, watch.lastStatusId, instructionId, lossRatio);
    }

    scheduleAfter(frameInterval, watch.timer);
}
