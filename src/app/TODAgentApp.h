//
// Copyright (C) 2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef _TODAGENTAPP_H
#define _TODAGENTAPP_H
#include <omnetpp.h>
#include "inet/networklayer/common/L3Address.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "inet/common/lifecycle/OperationalBase.h"
#include "inet/applications/base/ApplicationBase.h"

#include "../carla_omnet/CarlaCommunicationManager.h"
#include "messages/TodMessages_m.h"

using namespace omnetpp;
using namespace inet;


class ProcessStatusTimeFilter : public cObjectResultFilter{
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details) override;
};

Register_ResultFilter("processStatusTime", ProcessStatusTimeFilter);


/**
 * UDP application. See NED for more info.
 */
class TODAgentApp : public ApplicationBase, public UdpSocket::ICallback
{
private:
    const int PROCESS_STATUS_MESSAGE_KIND = 1;
    CarlaCommunicationManager* carlaCommunicationManager;
    string agentId;

    map<pair<string,string>, int> reassembleStatusPacketsMap;

    bool reassembleStatusPacket(string actorId, string statusId, int numFragments); //returns true if all fragments have been received

protected:
    UdpSocket socket;
    // statistics
    int numSent = 0;
    int numReceived = 0;


protected:
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;

    //handle application logic
    virtual void handleStatusUpdateMessage(Packet *packet);//Statconst char *actorId, const char *statusId, L3Address srcAddr, int srcPort);
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;
    virtual void refreshDisplay() const override;

    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;
    virtual void calcAndSendnstruction(ProcessedStatusMessage *todStatusMessage);
    /*UDP logic*/
    /**
     * Notifies about data arrival, packet ownership is transferred to the callee.
     */
    virtual void socketDataArrived(UdpSocket *socket, Packet *packet);

    /**
     * Notifies about error indication arrival, indication ownership is transferred to the callee.
     */
    virtual void socketErrorArrived(UdpSocket *socket, Indication *indication);

    /**
     * Notifies about socket closed, indication ownership is transferred to the callee.
     */
    virtual void socketClosed(UdpSocket *socket);


    virtual void sendPacket(Packet *packet, L3Address addr, int port);
    virtual void processPacket(Packet *pk);


public:
    ~TODAgentApp();
};


#endif

