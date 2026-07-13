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

#include "../carla_omnet/TodCarlanetManager.h"
#include "inet/networklayer/common/L3Address.h"
#include "inet/transportlayer/contract/quic/QuicSocket.h"
#include "inet/common/lifecycle/OperationalBase.h"
#include "inet/applications/base/ApplicationBase.h"

#include "messages/TodMessages_m.h"

using namespace omnetpp;
using namespace inet;


class ProcessStatusTimeFilter : public cObjectResultFilter{
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details) override;
};

Register_ResultFilter("processStatusTime", ProcessStatusTimeFilter);


/**
 * QUIC teleoperator (agent) application. See NED for more info.
 */
class TODAgentApp : public ApplicationBase, public QuicSocket::ICallback
{
private:
    const int PROCESS_STATUS_MESSAGE_KIND = 1;
    TodCarlanetManager* carlaCommunicationManager;
    string agentId;

    map<pair<string,string>, int> reassembleStatusPacketsMap;

    bool reassembleStatusPacket(string actorId, string statusId, int numFragments); //returns true if all fragments have been received

    struct FrameAcc
    {
        int expected = -1;
        int arrived = 0;
    };
    std::map<uint64_t, FrameAcc> frameAcc;
    uint64_t lastClosedFrame = 0;   // closing frame id to ignore all other packets

    void countSensorData(Packet *packet);
    double computeLossRatio(uint64_t frameId);

protected:
    QuicSocket socket;                                      // listening socket
    std::vector<QuicSocket*> clientSockets;                 // accepted connections
    std::map<std::string, QuicSocket*> replySocketByActor;  // connection to reply on, per actor
    // statistics
    int numSent = 0;
    int numReceived = 0;


protected:
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;

    //handle application logic
    virtual void handleStatusUpdateMessage(QuicSocket *socket, Packet *packet);
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;
    virtual void refreshDisplay() const override;

    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;
    virtual void calcAndSendnstruction(ProcessedStatusMessage *todStatusMessage);

    /* QUIC socket callbacks (QuicSocket::ICallback) */
    virtual void socketDataArrived(QuicSocket *socket, Packet *packet) override;
    virtual void socketDataAvailable(QuicSocket *socket, QuicDataInfo *dataInfo) override;
    virtual void socketConnectionAvailable(QuicSocket *socket) override;
    virtual void socketEstablished(QuicSocket *socket) override { }
    virtual void socketClosed(QuicSocket *socket) override;
    virtual void socketDestroyed(QuicSocket *socket) override;
    virtual void socketSendQueueFull(QuicSocket *socket) override { }
    virtual void socketSendQueueDrain(QuicSocket *socket) override { }
    virtual void socketMsgRejected(QuicSocket *socket) override { }

    virtual void sendPacket(QuicSocket *socket, Packet *packet, uint64_t streamId);
    virtual void processPacket(QuicSocket *socket, Packet *pk, int streamID);


public:
    ~TODAgentApp();
};


#endif
