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

#include "inet/networklayer/common/L3Address.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "inet/applications/base/ApplicationBase.h"

#include "../carla_omnet/CarlaCommunicationManager.h"
#include "messages/TodMessages_m.h"
//#include "utils/InstructionDelayResultFilter.h"
using namespace omnetpp;
using namespace inet;


class instructionRTTNetworkFilter : public cObjectResultFilter{
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details) override;
};


class InstructionDelayResultFilter : public cObjectResultFilter{
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details) override;
};

class RetrievalStatusFilter : public cObjectResultFilter{
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details) override;
};


Register_ResultFilter("instructionRTTNetwork", instructionRTTNetworkFilter);
Register_ResultFilter("InstructionDelay", InstructionDelayResultFilter);
Register_ResultFilter("RetrievalStatus", RetrievalStatusFilter);




/**
 * UDP application. See NED for more info.
 */
class TODCarApp : public ApplicationBase, public UdpSocket::ICallback
{

private:
    CarlaCommunicationManager* carlaCommunicationManager;
    cMessage* updateStatusSelfMessage;
    double statusUpdateInterval;
    const char *actorId;
    const char *RETRIEVE_STATUS_DATA_MSG_NAME = "RETRIEVE_STATUS_DATA";

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


    virtual void retrieveStatusData();
    /*Application logic*/
    virtual void sendUpdateStatusPacket(simtime_t dataRetrievalTime);

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

