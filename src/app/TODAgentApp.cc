//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004,2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "TODAgentApp.h"

#include "../sensors/messages/SensorMessages_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/TagBase_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/quic/QuicCommand_m.h"

#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"

using namespace omnetpp;
using namespace inet;
using namespace std;

Define_Module(TODAgentApp);


void ProcessStatusTimeFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet*>(object);

    if (strncmp(packet->getName(), "SensorDatagram", 14) != 0)
    {
        return;
    }

    if (strncmp(packet->getName(), "StatusUpdate", 12) != 0)
    {
        return;
    }

    simtime_t retrievalTime = packet->peekData<TodInstructionMessage>()->getStatusProcessingTime();

    auto instrucionDelay = simTime() - retrievalTime;

    fire(this, simTime(), instrucionDelay,  details);
}


TODAgentApp::~TODAgentApp()
{
    for (QuicSocket *cs : clientSockets)
    {
        delete cs;
    }
}

void TODAgentApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL)
    {
        agentId = par("agentId").stdstringValue();
        carlaCommunicationManager = check_and_cast<TodCarlanetManager*>(getParentModule()->getParentModule()->getSubmodule("carlaCommunicationManager"));
    }
}


void TODAgentApp::handleMessageWhenUp(cMessage* msg)
{
    if (msg->isSelfMessage())
    {
        if (msg->getKind() == PROCESS_STATUS_MESSAGE_KIND)
        {
            ProcessedStatusMessage *pkt = dynamic_cast<ProcessedStatusMessage *>(msg);
            calcAndSendnstruction(pkt);
            delete pkt;   // self-message gia' scattato: va liberato, altrimenti leaka
                          // un ProcessedStatusMessage per ogni status (undisposed a fine run)
        }
    }
    else if (socket.belongsToSocket(msg))
    {
        socket.processMessage(msg);
    }
    else
    {
        for (QuicSocket *cs : clientSockets)
        {
            if (cs->belongsToSocket(msg))
            {
                cs->processMessage(msg);
                break;
            }
        }
    }
}


void TODAgentApp::calcAndSendnstruction(ProcessedStatusMessage *todStatusMessage)
{
    auto actorId = todStatusMessage->getActorId();
    auto statusId = todStatusMessage->getStatusId();
    auto statusCreationTime = todStatusMessage->getStatusCreationTime();
    auto statusCollectionTime = todStatusMessage->getCollectionTime();

    EV_INFO << "handleStatusUpdateMessage " << actorId << "," << statusId << endl;

    double lossRatio = computeLossRatio(todStatusMessage->getFrameId());

    auto instructionId = carlaCommunicationManager->computeInstruction(actorId, statusId, agentId, lossRatio);

    auto packet = new Packet("Instruction");
    auto data = makeShared<TodInstructionMessage>();

    data->setActorId(actorId);
    data->setInstructionId(instructionId.c_str());
    data->setStatusCreationTime(statusCreationTime);
    data->setStatusDataCollectionTime(statusCollectionTime);
    data->setStatusProcessingTime(todStatusMessage->getTimestamp());
    data->setInstructionCreationTime(simTime());
    data->setLossRatio(lossRatio);

    auto creationTimeTag = data->addTag<CreationTimeTag>();
    creationTimeTag->setCreationTime(simTime());

    // Lunghezza fissa e byte-allineata per l'istruzione (come statusMessageLength
    // lato TODCarApp). I messaggi FieldsChunk nascono con chunkLength = b(-1)
    // (non impostata): senza questa riga QUIC, nel framing su stream 0, esegue
    // B(region.length) su un valore non multiplo di 8 e lancia
    // "Cannot convert between integer units".
    data->setChunkLength(B(par("instructionMessageLength").intValue()));

    packet->insertAtBack(data);

    auto actorToReply = replySocketByActor.find(actorId);
    if (actorToReply != replySocketByActor.end())
    {
        sendPacket(actorToReply->second, packet, 0);
    }
    else
    {
        EV_WARN << "No reply socket for actor " << actorId << ", dropping instruction" << endl;
        delete packet;
    }
}


void TODAgentApp::refreshDisplay() const{}

void TODAgentApp::finish()
{
    ApplicationBase::finish();
}

void TODAgentApp::handleStartOperation(LifecycleOperation *operation)
{
    L3Address localAddress;
    L3AddressResolver().tryResolve(par("localAddress"), localAddress);
    int localPort = par("localPort");
    socket.setOutputGate(gate("socketOut"));
    socket.bind(localAddress, localPort);
    socket.listen(); // QUIC server: accept incoming connections
    socket.setCallback(this);
}


void TODAgentApp::handleStopOperation(LifecycleOperation *operation)
{
    socket.close();
    for (QuicSocket *cs : clientSockets)
    {
        cs->close();
    }
}

void TODAgentApp::handleCrashOperation(LifecycleOperation *operation)
{
    socket.setCallback(nullptr);
    for (QuicSocket *cs : clientSockets)
    {
        cs->setCallback(nullptr);
    }
}


void TODAgentApp::socketConnectionAvailable(QuicSocket *socket)
{
    // QUIC server: a new connection is available, accept it.
    QuicSocket *clientSocket = socket->accept();
    clientSocket->setCallback(this);
    clientSockets.push_back(clientSocket);
    EV_INFO << "TODAgentApp: accepted connection, client socket " << clientSocket->getSocketId() << endl;
}


void TODAgentApp::socketDataAvailable(QuicSocket *socket, QuicDataInfo *dataInfo)
{
    // Lo status viaggia su stream 0 come messaggi discreti di dimensione fissa
    // (TODCarApp fa setChunkLength(B(statusMessageLength))). QUIC pero' e' un byte
    // stream: sotto carico di datagram sensori il frame STREAM dello status viene
    // spezzato su piu' pacchetti (PacketBuilder impacchetta prima i DATAGRAM, poi lo
    // stream nello spazio residuo). Se leggessimo i byte parziali disponibili
    // consegneremmo un TodStatusUpdateMessage INCOMPLETO, e processPacket lancerebbe
    // "Cannot convert chunk ... to inet::SensorDataResponse" (hasData<TODMessage>()
    // e' false su un chunk incompleto e si cade nel ramo SensorDataResponse).
    // Quindi leggiamo solo quando e' disponibile un messaggio COMPLETO, uno per volta;
    // i byte restanti restano bufferati in QUIC e una nuova notifica arrivera' col
    // prossimo frame.
    const int64_t msgLen = par("statusMessageLength").intValue();
    if ((int64_t) dataInfo->getAvaliableDataSize() >= msgLen)
        socket->recv(msgLen, dataInfo->getStreamID());
}

void TODAgentApp::socketDataArrived(QuicSocket *socket, Packet *packet)
{
    emit(packetReceivedSignal, packet);
    auto streamReq = packet->findTag<QuicStreamReq>();
    int streamID = streamReq ? streamReq->getStreamID() : 0;

    EV_INFO << "Received packet from stream: " << streamID << endl;

    processPacket(socket, packet, streamID);

    delete packet;
    numReceived++;
}

void TODAgentApp::socketDatagramArrived(QuicSocket *socket, Packet *packet)
{
    // Sensors data arrive as QUIC Datagram
    emit(packetReceivedSignal, packet);
    countSensorData(packet);
    delete packet;
    numReceived++;
}


void TODAgentApp::socketClosed(QuicSocket *socket) {}

void TODAgentApp::socketDestroyed(QuicSocket *socket)
{
    if (socket == &this->socket)
    {
        return;
    }

    // Clears the socket from the open sockets vector
    for (vector<QuicSocket*>::iterator socketIterator = clientSockets.begin(); socketIterator != clientSockets.end(); ++socketIterator)
    {
        if (*socketIterator == socket)
        {
            clientSockets.erase(socketIterator);
            break;
        }
    }

    // Clears the socket from the map actorId -> socket
    for (auto actorSocket = replySocketByActor.begin(); actorSocket != replySocketByActor.end(); )
    {
        if (actorSocket->second == socket)
        {
            actorSocket = replySocketByActor.erase(actorSocket);
        }
        else
        {
            ++actorSocket;
        }
    }

    delete socket;
}

void TODAgentApp::sendPacket(QuicSocket *socket, Packet *packet, uint64_t streamId)
{
    emit(packetSentSignal, packet);
    socket->send(packet, streamId);
    numSent++;
}


void TODAgentApp::handleStatusUpdateMessage(QuicSocket *socket, Packet *statusPacket)
{
    auto todStatusMessage = statusPacket->peekData<TodStatusUpdateMessage>();
    auto actorId = todStatusMessage->getActorId();
    auto statusId = todStatusMessage->getStatusId();

    EV_INFO << "Received message for "<< actorId << " Status "<< statusId << endl;

    replySocketByActor[actorId] = socket; // Socket update for the given actor

    uint64_t frameId = todStatusMessage->getFrameId();
    auto& frameAcc = frameStats[frameId];

    // Flush of the old streams and new setup
    frameAcc.expectedStreams.clear();
    for (size_t i = 0; i < todStatusMessage->getExpectedStreamsArraySize(); i++)
    {
        frameAcc.expectedStreams.push_back(todStatusMessage->getExpectedStreams(i));
    }

    ProcessedStatusMessage *packet = new ProcessedStatusMessage("processStatusMessage", PROCESS_STATUS_MESSAGE_KIND);
    packet->setActorId(todStatusMessage->getActorId());
    packet->setStatusId(todStatusMessage->getStatusId());
    packet->setStatusCreationTime(statusPacket->peekData<TodStatusUpdateMessage>()->getAllTags<CreationTimeTag>()[0].getTag()->getCreationTime());
    packet->setCollectionTime(todStatusMessage->getCollectionTime());
    packet->setFrameId(frameId);
    packet->setTimestamp();

    double processingStatusTime = par("processingStatusTime");
    scheduleAfter(processingStatusTime, packet);
}


void TODAgentApp::processPacket(QuicSocket *socket, Packet *packet, int streamID)
{
    // hasData<T>()/peekData<T>() LANCIANO se il chunk non e' del tipo richiesto
    // (convertChunk rifiuta la reinterpretazione di un FieldsChunk). Se su stream 0
    // arriva un chunk non "pulito" (SequenceChunk/SliceChunk/...) il vecchio codice
    // crashava con "Cannot convert ... to SensorDataResponse". Discriminiamo sul chunk
    // grezzo con dynamicPtrCast: NON converte, NON lancia. Entriamo nel ramo status solo
    // se il data part e' ESATTAMENTE un TODMessage, cosi' anche il peekData<TodStatusUpdateMessage>()
    // dentro handleStatusUpdateMessage non puo' lanciare.
    auto dataChunk = packet->peekData<Chunk>();
    if (auto todMsg = dynamicPtrCast<const TODMessage>(dataChunk))
    {
        if (todMsg->getMessageType() == TODMessageType::STATUS)
        {
            EV_INFO << "Received status/control (stream " << streamID << ")" << endl;
            handleStatusUpdateMessage(socket, packet);
        }
        else
            EV_WARN << "Received an unexpected TOD Message " << todMsg->getMessageType() << " check your implementation" << endl;
    }
    else if (dynamicPtrCast<const SensorDataResponse>(dataChunk))
    {
        countSensorData(packet);
    }
    else
    {
        // DIAGNOSTICA: invece di crashare logghiamo COSA arriva di inatteso, per inchiodare
        // il bug deterministico a t=14.511s. Cmdenv express mode nasconde EV_*, quindi cout.
        // Cap a 20 righe per non spammare se il problema fosse persistente.
        static int unexpectedCount = 0;
        if (++unexpectedCount <= 20)
            std::cout << "[TODAgentApp] unexpected chunk on stream " << streamID
                      << " t=" << simTime()
                      << " name=" << packet->getName()
                      << " dataLen=" << packet->getDataLength()
                      << " chunkType=" << (dataChunk ? dataChunk->getClassName() : "null")
                      << std::endl;
    }
}


void TODAgentApp::countSensorData(Packet *packet)
{
    // a sensor datagram is [SensorDataResponse header | ByteCountChunk payload]
    // (a SequenceChunk), so read only the FRONT header, not the whole packet.
    auto data = packet->peekAtFront<SensorDataResponse>();
    uint64_t frameId = data->getFrameId();

    if (frameId <= lastClosedFrame)
    {
        EV_INFO << "Datagram sensor frame " << frameId << " arrived after deadline" << endl;
        return;
    }

    auto& stats = frameStats[frameId].statsPerStream[data->getStreamId()];
    stats.arrived++;
    stats.total = (int) data->getTotalFragments();
    EV_INFO << "Datagram sensor frame " << frameId << " stream " << data->getStreamId()
            << " fragment " << data->getFragmentNum() << "/" << data->getTotalFragments()
            << " (arrived " << stats.arrived << ")" << endl;
}


double TODAgentApp::computeLossRatio(uint64_t frameId)
{
    if (frameId > lastClosedFrame) lastClosedFrame = frameId;

    auto frameAcc = frameStats.find(frameId);
    if (frameAcc == frameStats.end())
    {
        return 0.0;
    }

    FrameAcc &frameAccStats = frameAcc->second;

    double sumWeighted = 0.0, sumWeightedLoss = 0.0;
    for (uint64_t streamId : frameAccStats.expectedStreams)
    {
        double loss_s;
        auto streamStats = frameAccStats.statsPerStream.find(streamId);

        if (streamStats == frameAccStats.statsPerStream.end() || streamStats->second.arrived == 0)
        {
            loss_s = 1.0;
        }
        else
        {
            int total = streamStats->second.total;
            int arrived = streamStats->second.arrived;

            if (total <= 0)
            {
                loss_s = 0.0;
            }
            else
            {
                if (arrived > total)
                {
                    arrived = total;
                }

                loss_s = 1.0 - (double) arrived / (double) total;
            }
        }
        double weightPerSensor = 1.0; // With 1 all sensors have the same weight
        sumWeighted += weightPerSensor;
        sumWeightedLoss += weightPerSensor * loss_s;
    }

    double loss = (sumWeighted > 0.0) ? sumWeightedLoss / sumWeighted : 0.0;

    EV_INFO << "Frame " << frameId << ": weighted lossRatio " << loss
            << " (expected sensors " << frameAccStats.expectedStreams.size() << ")" << endl;

    frameStats.erase(frameAcc);
    return loss;
}
