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

void SensorBaseApp::initialize() {
    streamID = par("streamID");
    sensorType = par("sensorType").stdstringValue();
    updateInterval = par("updateInterval");
}

void SensorBaseApp::handleMessage(cMessage *msg) {
    if (msg->arrivedOn("fromManager")) infoFromPacket(msg);
    else delete msg;
}

void SensorBaseApp::infoFromPacket(cMessage* msg) {
    Packet *pkt = check_and_cast<Packet*>(msg);

    auto req = pkt->peekAtFront<SensorDataRequest>();
    lastFrameId = req->getFrameId();
    delete pkt;

    if (firstEmit || simTime() - lastEmitTime >= updateInterval) {
        sendData();
        lastEmitTime = simTime();
        firstEmit = false;
    }
}

void SensorBaseApp::sendData() {
    long dataSize = (long) par("dataSize").doubleValue();

    auto pkt = new Packet("SensorData");

    auto resp = makeShared<SensorDataResponse>();
    resp->setStreamId(streamID);
    resp->setSensorType(sensorType.c_str());
    resp->setCollectionTime(simTime());
    resp->setChunkLength(B(16));
    pkt->insertAtBack(resp);

    if (dataSize > 0)
        pkt->insertAtBack(makeShared<ByteCountChunk>(B(dataSize)));

    EV_INFO << getFullName() << " (" << sensorType << ") emette " << dataSize
            << " B su stream " << streamID << endl;
    send(pkt, "toManager");
}
