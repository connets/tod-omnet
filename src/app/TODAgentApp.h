//
// Copyright (C) 2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef _TODAGENTAPP_H
#define _TODAGENTAPP_H
#include <omnetpp.h>
#include <vector>
#include <map>
#include <set>
#include <string>

#include "../carla_omnet/TodCarlanetManager.h"
#include "inet/networklayer/common/L3Address.h"
#include "inet/transportlayer/contract/quic/QuicSocket.h"
#include "inet/common/lifecycle/OperationalBase.h"
#include "inet/applications/base/ApplicationBase.h"

#include "messages/TodMessages_m.h"

using namespace omnetpp;
using namespace inet;
using namespace std;

class ProcessStatusTimeFilter : public cObjectResultFilter{
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details) override;
};

Register_ResultFilter("processStatusTime", ProcessStatusTimeFilter);

class TODAgentApp : public ApplicationBase, public QuicSocket::ICallback
{
private:
    const int CLOSE_FRAME_MSG_KIND = 1;             // per-frame close timer
    const int WATCHDOG_MSG_KIND = 2;                // per-actor 100%-loss detector
    TodCarlanetManager* carlaCommunicationManager;
    string agentId;
    double reuseDecay;

    struct SensorAcc
    {
        int arrived = 0;
        int total = 0;
    };

    struct FrameAcc
    {
        string actorId;
        vector<uint64_t> expectedStreams;
        map<uint64_t, SensorAcc> statsPerStream;
        simtime_t collectionTime;
        simtime_t firstArrivalTime;
        cMessage* closeTimer = nullptr;
    };
    map<string, FrameAcc> openFrames;
    set<string> closedFrames;


    struct StreamHistory
    {
        double lastGoodRatio = 0.0;                    // fraction received the last time it delivered
        uint64_t expectCount = 0;                      // how many times this stream has been expected
        uint64_t lastGoodExpectCount = 0;              // expectCount when it last delivered
        bool hasHistory = false;
    };
    map<string, map<uint64_t, StreamHistory>> streamHistory;   // actorId -> streamId -> history


    struct ActorWatch
    {
        ProcessedStatusMessage* timer = nullptr;
        simtime_t lastFrameOpenTime = 0;        // when the last frame opened (any datagram)
        string lastStatusId;                    // last statusId that produced an instruction
        vector<uint64_t> lastExpectedStreams;   // expected set of the last received frame
    };
    map<string, ActorWatch> actorWatch;         // actorId -> watchdog state

protected:
    QuicSocket socket;                                 // listening socket
    vector<QuicSocket*> clientSockets;                 // accepted connections
    map<string, QuicSocket*> replySocketByActor;       // connection to reply on, per actor

    // statistics
    int numSent = 0;
    int numReceived = 0;

private:
    void openFrame(const string& statusId, const string& actorId,
                   const vector<uint64_t>& expectedStreams, simtime_t collectionTime,
                   QuicSocket* replySocket);
    void countSensorData(QuicSocket* socket, Packet* packet);
    void closeFrame(const string& statusId);
    double computeLossRatio(FrameAcc& frame, const string& actorId);
    void createAndSendInstructionMessage(FrameAcc& frame, const string& statusId,
                                         const string& instructionId, double lossRatio);
    void watchdogTick(const string& actorId);

protected:
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;

    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;
    virtual void refreshDisplay() const override;

    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

    /* QUIC socket callbacks (QuicSocket::ICallback) */
    virtual void socketDatagramArrived(QuicSocket *socket, Packet *packet) override;
    virtual void socketDataArrived(QuicSocket *socket, Packet *packet) override;
    virtual void socketDataAvailable(QuicSocket *socket, QuicDataInfo *dataInfo) override;
    virtual void socketConnectionAvailable(QuicSocket *socket) override;
    virtual void socketEstablished(QuicSocket *socket) override { }
    virtual void socketClosed(QuicSocket *socket) override;
    virtual void socketDestroyed(QuicSocket *socket) override;
    virtual void socketSendQueueFull(QuicSocket *socket) override { }
    virtual void socketSendQueueDrain(QuicSocket *socket) override { }
    virtual void socketMsgRejected(QuicSocket *socket) override { }

public:
    ~TODAgentApp();
};


#endif
