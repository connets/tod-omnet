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
#include "inet/transportlayer/contract/quic/QuicSocket.h"
#include "inet/applications/base/ApplicationBase.h"

#include "messages/TodMessages_m.h"
#include "carlanet/CarlaInetMobility.h"
using namespace omnetpp;
using namespace inet;


class instructionRTTNetworkFilter : public cObjectResultFilter{
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details) override;
};


class InstructionDelayResultFilter : public cObjectResultFilter{
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details) override;
};

class StatusCreationTime : public cObjectResultFilter{
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details) override;
};


Register_ResultFilter("instructionRTTNetwork", instructionRTTNetworkFilter);
Register_ResultFilter("instructionDelay", InstructionDelayResultFilter);
Register_ResultFilter("statusCreationTime", StatusCreationTime);




/**
 * QUIC application. See NED for more info.
 */
class TODCarApp : public ApplicationBase, public QuicSocket::ICallback
{

private:
    TodCarlanetManager* carlaCommunicationManager;
    cMessage* updateStatusSelfMessage;
    double statusUpdateInterval;
    const char *actorId;
    const int CREATION_STATUS_DATA_MSG_KIND = 2;
    bool zeroDelay;
    uint64_t frameCounter = 0;   // It resets in each flush of the status update

    std::vector<Packet*> sensorBuffer;

protected:
    QuicSocket socket;
    L3Address destAddress;
    int destPort;

private:
    virtual void applyZeroDelay();

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

    /* QUIC socket callbacks (QuicSocket::ICallback) */
    virtual void socketDataArrived(QuicSocket* socket, Packet* packet) override;
    virtual void socketDataAvailable(QuicSocket* socket, QuicDataInfo* dataInfo) override;
    virtual void socketEstablished(QuicSocket* socket) override;
    virtual void socketClosed(QuicSocket* socket) override;
    virtual void socketConnectionAvailable(QuicSocket* socket) override { }
    virtual void socketDestroyed(QuicSocket* socket) override { }
    virtual void socketSendQueueFull(QuicSocket* socket) override { }
    virtual void socketSendQueueDrain(QuicSocket* socket) override { }
    virtual void socketMsgRejected(QuicSocket* socket) override { }

    virtual void bufferizeSensorData(cMessage* msg);
    virtual void sendSensorPacket(Packet* pk, uint64_t frameId);
    virtual void sendUpdatePacket(Packet* pk);
    virtual void processPacket(Packet* pk);

public:
    ~TODCarApp();


};

#endif

