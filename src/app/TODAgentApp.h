//
// Copyright (C) 2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef _TODAGENTAPP_H
#define _TODAGENTAPP_H
#include <omnetpp.h>
#include <deque>
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
    const int SLOT_DEADLINE_MSG_KIND = 1;   // per-actor fixed-cadence deadline
    const int SLOT_DELIVER_MSG_KIND = 2;    // deadline + processing time
    // Instruction id meaning "no instruction": the CARLA side checks for it and
    // applies nothing, so it does not rearm the on-board dead-man's switch either.
    const char* NO_INSTRUCTION_ID = "-1";
    // Cap on how many settled statusIds we remember per actor to recognise late
    // datagrams. Bounded on purpose: an unbounded set grows for the whole run.
    const size_t SETTLED_HISTORY = 256;

    TodCarlanetManager* carlaCommunicationManager;
    string agentId;

    /*
     * The agent runs on its OWN clock.
     *
     * It does not open a window when data happens to arrive; it declares a
     * cadence, starts it at the first datagram it ever receives from an actor,
     * and from then on decides at fixed instants using whatever made the
     * deadline. Loss is then simply what did not arrive in time, and a slot in
     * which nothing arrived is 100% loss without any special case: there is no
     * watchdog, because there is nothing to detect.
     *
     * How long a fragment stays useful is a SEPARATE axis from when we decide. A
     * frame is viable until collectionTime + frameBudget, not until the end of the
     * window it happened to land in: a frame spread across two windows keeps
     * accumulating instead of being torn in half and half thrown away. That is a
     * playout budget, the same idea as a jitter buffer, and it is anchored to the
     * frame rather than to our clock.
     */
    simtime_t samplingInterval;
    simtime_t frameBudget;

    struct StreamAcc
    {
        int arrived = 0;
        int total = 0;
    };

    // One car-side frame (one statusId) as it accumulates inside the current slot.
    struct StatusAcc
    {
        string actorId;
        string statusId;
        vector<uint64_t> expectedStreams;
        map<uint64_t, StreamAcc> statsPerStream;
        simtime_t collectionTime;
        simtime_t firstArrivalTime;
        int qualityLevel = 0;   // camera quality this frame was produced at
    };

    struct ActorSlot
    {
        ProcessedStatusMessage* deadline = nullptr;  // periodic, never deleted on fire
        map<string, StatusAcc> open;                 // statusId -> accumulation, current slot
        set<string> settled;                         // statusIds already accounted for
        deque<string> settledOrder;                  // eviction order for settled
        deque<StatusAcc> toDeliver;                  // snapshots waiting out processingStatusTime
        simtime_t slotStart = 0;                     // start of the window now accumulating

        // Camera quality the agent is asking this actor for, from its own loss.
        double lossEwma = 0.0;
        bool hasLossSample = false;
        int requestedLevel = 0;
    };
    map<string, ActorSlot> actorSlots;              // actorId -> slot state

    /*
     * Quality ladder, decided HERE and not on the car.
     *
     * The car can only measure the round trip of the instructions that came back,
     * which says nothing about what was dropped on the way up: a heavily lossy
     * uplink with a healthy downlink looks perfectly fine from the car. The agent
     * is the only side that knows what did not arrive, so it is the side that
     * picks the level; the request rides back on the instruction.
     *
     * Thresholds are loss fractions at which we step DOWN one level. Stepping back
     * up needs the loss to fall below the threshold that took us down, shrunk by
     * the hysteresis band, so a link on a boundary settles instead of flapping.
     */
    vector<double> qualityLossThresholds;
    double lossEwmaAlpha = 0.0;
    double qualityHysteresis = 0.0;

    static simsignal_t lossRatioEwmaSignal;
    static simsignal_t requestedQualityLevelSignal;

protected:
    QuicSocket socket;                                 // listening socket
    vector<QuicSocket*> clientSockets;                 // accepted connections
    map<string, QuicSocket*> replySocketByActor;       // connection to reply on, per actor

    // statistics
    int numSent = 0;
    int numReceived = 0;

private:
    void countSensorData(QuicSocket* socket, Packet* packet);
    void startSlotClock(const string& actorId);
    void closeSlot(const string& actorId);
    void deliverSlot(const string& actorId);
    void settle(ActorSlot& slot, const string& statusId);
    double computeLossRatio(const StatusAcc& status) const;
    double completeness(const StatusAcc& status) const;
    void sendInstruction(const StatusAcc& status, const string& instructionId, double lossRatio,
                         int requestedQualityLevel);
    void dropActor(const string& actorId);
    void parseQualityLossThresholds(const char* spec);
    int updateRequestedQuality(ActorSlot& slot, const string& actorId, double lossRatio);

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
