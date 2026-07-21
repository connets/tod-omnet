//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004,2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "SensorBaseApp.h"
#include "messages/SensorMessages_m.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"

Define_Module(SensorBaseApp);

SensorBaseApp::~SensorBaseApp() {}

void SensorBaseApp::initialize()
{
    streamID = par("streamID");
    sensorType = par("sensorType").stdstringValue();
    updateInterval = par("updateInterval");
}

void SensorBaseApp::handleMessage(cMessage *msg)
{
    if (msg->arrivedOn("fromManager"))
    {
        infoFromPacket(msg); // Gets all the info arrived from the manager
    }
    else
    {
        delete msg;
    }
}

void SensorBaseApp::infoFromPacket(cMessage* msg)
{
    Packet *packet = check_and_cast<Packet*>(msg);

    auto request = packet->peekAtFront<SensorDataRequest>();
    lastFrameId = packet->getFrameId();
    delete packet;

    /*
     * This if controls if the sensor can send or not the data
     * according to its updateInterval.
     *
     * If it's the first message ever from the current sensor or
     * if the updateInterval is elapsed then it sends the data
     */
    if (firstEmit || simTime() - lastEmitTime >= updateInterval)
    {
        sendData();
        lastEmitTime = simTime();
        firstEmit = false;
    }
}

void SensorBaseApp::sendData()
{
    long dataSize = (long) par("dataSize").doubleValue(); // It's different for each sensor

    auto packet = new Packet("SensorData");

    auto response = makeShared<SensorDataResponse>();
    response->setStreamId(streamID);
    response->setSensorType(sensorType.c_str());
    response->setCollectionTime(simTime());
    response->setChunkLength(B(16));
    packet->insertAtBack(response);

    if (dataSize > 0)
    {
        packet->insertAtBack(makeShared<ByteCountChunk>(B(dataSize)));
    }

    EV_INFO << getFullName() << " (" << sensorType << ") sends " << dataSize
            << " Byte su stream " << streamID << endl;
    send(packet, "toManager");
}
