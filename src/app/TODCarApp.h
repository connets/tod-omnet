//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004,2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __TODCARAPP_H
#define __TODCARAPP_H

#include <vector>
#include <omnetpp.h>

#include "../carla_omnet/TodCarlanetManager.h"
#include "inet/networklayer/common/L3Address.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "inet/applications/base/ApplicationBase.h"

#include "messages/TodMessages_m.h"
//#include "utils/InstructionDelayResultFilter.h"
using namespace omnetpp;
using namespace inet;


class InstructionDelayResultFiter : public cObjectResultFilter{
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details) override;
};


Register_ResultFilter("instructionDelay", InstructionDelayResultFiter);




/**
 * UDP application. See NED for more info.
 */
class TODCarApp : public ApplicationBase, public UdpSocket::ICallback
{

private:
    TodCarlanetManager* carlaCommunicationManager;
    cMessage* updateStatusSelfMessage;
    double statusUpdateInterval;
    const char *actorId;

protected:
    UdpSocket socket;
    L3Address destAddress;
    int destPort;
    // statistics
    int numSent = 0;
    int numReceived = 0;


protected:
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;
    virtual void refreshDisplay() const override;

    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;


    /*Application logic*/
    virtual void sendUpdateStatusPacket();

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


    virtual void sendPacket(Packet *pk);
    virtual void processPacket(Packet *pk);

public:
    ~TODCarApp();


};

#endif

