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
using namespace std;


class instructionRTTNetworkFilter : public cObjectResultFilter{
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details) override;
};


class InstructionDelayResultFilter : public cObjectResultFilter{
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details) override;
};

class StatusCreationTime : public cObjectResultFilter{
    string lastFiredStatusId;   // fire the per-frame stat only once per statusId
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
    // uint64_t frameCounter = 0; // It resets in each flush of the status update

    vector<Packet*> sensorBuffer;
    set<uint64_t> streamsThisFrame;

    struct SensorSource
    {
        string statusId;
        string carlaId;
        uint64_t streamId;
        string sensorType;
        simtime_t collectionTime;
        int64_t headerBytes;
        int qualityLevel;
    };

    SensorSource currentSource;

    /*
     * Adaptive camera quality.
     *
     * The car measures how stale the instruction it just received is: the time
     * between the sampling of the frame that produced it and its arrival here.
     * That delay, smoothed with an EWMA, is what the operator actually suffers,
     * and it is what selects the camera quality level. The level then travels
     * down to the sensors, which cut the bytes they put on the wire, so the
     * loop closes: a slower link asks for a smaller frame, which makes the link
     * faster again.
     *
     * Thresholds are the RTT at which we step DOWN to the next level. Stepping
     * back up requires the RTT to fall below the threshold shrunk by the
     * hysteresis band, so a link sitting right on a boundary does not flap.
     */
    vector<simtime_t> qualityRttThresholds;
    double qualityHysteresis = 0.0;
    double rttEwmaAlpha = 0.0;
    simtime_t instructionRttEwma = SIMTIME_ZERO;
    simtime_t lastInstructionArrival = SIMTIME_ZERO;
    bool hasRttSample = false;
    int qualityLevel = 0;

    static simsignal_t instructionRttEwmaSignal;
    static simsignal_t qualityLevelSignal;

protected:
    QuicSocket socket;
    L3Address destAddress;
    int destPort;

private:
    virtual void applyZeroDelay();
    virtual void parseQualityRttThresholds(const char *spec);
    virtual void updateQualityLevel(simtime_t instructionRtt);
    virtual void degradeOnSilence();
    virtual void createAndSendFragmentPacket(int totalFragments, int64_t dataBytes, int64_t chunkSize);

    template <typename SourcePtr>
    void infoFromSource(const SourcePtr &source, string statusId, string carlaId)
    {
        currentSource = {};
        currentSource.statusId = statusId;
        currentSource.carlaId = carlaId;
        currentSource.streamId = source->getStreamId();
        currentSource.sensorType = source->getSensorType();
        currentSource.collectionTime = source->getCollectionTime();
        currentSource.headerBytes = B(source->getChunkLength()).get();
        // level the sensor actually produced at, not the one we asked for
        currentSource.qualityLevel = source->getQualityLevel();
    }

    /*
     * If dataBytes are less or equal then zero there's 1 fragment; otherwise
     * it returns the number of fragments.
     *
     * (dataBytes + chunkSize - 1): make sure to get more fragment if the dataBytes
     * are not multiple of chunkSize
     */
    int fragmentNumber(int64_t dataBytes, int64_t chunkSize)
    {
        return (dataBytes <= 0) ? 1 : (int) ((dataBytes + chunkSize - 1) / chunkSize);
    }

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
    virtual void retrieveStatusData();
    virtual void sendUpdateStatusPacket(simtime_t dataRetrievalTime);

    /* QUIC socket callbacks (QuicSocket::ICallback) */
    virtual void socketDatagramArrived(QuicSocket* socket, Packet* packet) override;
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
    virtual void sendSensorPacket(Packet* pk, string statusId, string carlaId);
    virtual void processPacket(Packet* pk);

public:
    ~TODCarApp();


};

#endif

