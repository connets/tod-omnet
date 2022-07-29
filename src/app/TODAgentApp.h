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

#include "../carla_omnet/CarlaCommunicationManager.h"
#include "messages/TodMessages_m.h"

using namespace omnetpp;
using namespace inet;

/**
 * UDP application. See NED for more info.
 */
class TODAgentApp : public cSimpleModule, public UdpSocket::ICallback
{
private:
    CarlaCommunicationManager* carlaCommunicationManager;
    string agentId;

protected:
    UdpSocket socket;
    // statistics
    int numSent = 0;
    int numReceived = 0;


protected:
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage* msg) override;

    //handle application logic
    virtual void handleStatusUpdateMessage(const char *actorId, const char *statusId, L3Address srcAddr, int srcPort);


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

