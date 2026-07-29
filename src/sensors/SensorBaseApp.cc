//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004,2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "SensorBaseApp.h"

#include <cstdio>

#include "messages/SensorMessages_m.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"

Define_Module(SensorBaseApp);

SensorBaseApp::~SensorBaseApp() {}

void SensorBaseApp::initialize()
{
    streamID = par("streamID");
    sensorType = par("sensorType").stdstringValue();
    updateInterval = par("updateInterval");
    parseQualityLadder(par("qualityResolutions").stringValue());
}

/*
 * Parses "1280x720 960x540 640x360" into rungs, ordered from best to worst, and
 * precomputes each rung's byte scale as its pixel count relative to rung 0.
 * An empty string leaves the ladder empty: the sensor is then non adaptive.
 */
void SensorBaseApp::parseQualityLadder(const char *spec)
{
    qualityLadder.clear();
    if (spec == nullptr || *spec == '\0')
    {
        return;
    }

    cStringTokenizer tokenizer(spec);
    while (tokenizer.hasMoreTokens())
    {
        const char *token = tokenizer.nextToken();
        int width = 0;
        int height = 0;
        if (sscanf(token, "%dx%d", &width, &height) != 2 || width <= 0 || height <= 0)
        {
            throw cRuntimeError("SensorBaseApp: bad entry '%s' in qualityResolutions, expected WIDTHxHEIGHT", token);
        }

        QualityRung rung;
        rung.width = width;
        rung.height = height;
        qualityLadder.push_back(rung);
    }

    double basePixels = (double) qualityLadder[0].width * (double) qualityLadder[0].height;
    for (auto& rung : qualityLadder)
    {
        rung.pixelScale = ((double) rung.width * (double) rung.height) / basePixels;
    }
}

/*
 * Byte scale for a level. Levels beyond the last rung clamp to the worst rung, so
 * a car that degrades further than the ladder describes simply stays at the floor.
 */
double SensorBaseApp::qualityScale(int level) const
{
    if (qualityLadder.empty())
    {
        return 1.0;
    }

    if (level < 0)
    {
        level = 0;
    }
    if (level >= (int) qualityLadder.size())
    {
        level = (int) qualityLadder.size() - 1;
    }

    return qualityLadder[level].pixelScale;
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
    // lastFrameId = packet->getFrameId();
    // Quality level decided by the car from the measured instruction RTT.
    currentQualityLevel = request->getQualityLevel();
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
    /*
     * dataSize is the nominal payload at quality level 0. The requested level
     * scales it by the pixel ratio of the corresponding rung: dropping from
     * 1280x720 to 640x360 is a quarter of the pixels, hence a quarter of the
     * bytes on the wire. dataSize stays volatile, so the per-frame jitter of the
     * configured distribution survives the scaling.
     */
    double scale = qualityScale(currentQualityLevel);
    long dataSize = (long) (par("dataSize").doubleValue() * scale); // It's different for each sensor

    auto packet = new Packet("SensorData");

    auto response = makeShared<SensorDataResponse>();
    response->setStreamId(streamID);
    response->setSensorType(sensorType.c_str());
    response->setCollectionTime(simTime());
    response->setQualityLevel(currentQualityLevel);
    response->setChunkLength(B(16));
    packet->insertAtBack(response);

    if (dataSize > 0)
    {
        packet->insertAtBack(makeShared<ByteCountChunk>(B(dataSize)));
    }

    EV_INFO << getFullName() << " (" << sensorType << ") sends " << dataSize
            << " Byte su stream " << streamID << " (livello " << currentQualityLevel
            << ", scala " << scale << ")" << endl;
    send(packet, "toManager");
}
