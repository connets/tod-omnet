//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//
import inet.common.packet.chunk.Chunk;

namespace inet;

//
// Intra-vehicle chunks (sensor <-> SensorManager <-> TODCarApp).
// These NEVER leave the vehicle: they are inserted as chunks into a Packet on
// the internal gates, so they MUST be FieldsChunk (not `message`).
//

// Poll sent by the SensorManager towards a single sensor.
class SensorDataRequest extends FieldsChunk
{
    simtime_t requestTime;
}

// Raw answer of a sensor to the SensorManager (then forwarded to the car).
class SensorDataResponse extends FieldsChunk
{
    uint64_t streamId;
    string sensorType;
    simtime_t collectionTime;
}

//
// Network chunks (TODCarApp <-> TODAgentApp over QUIC DATAGRAM).
// Everything is correlated by statusId (the opaque CARLA status id): there is
// no more numeric frameId and no more stream 0.
//

// One fragment of a sensor payload. Every fragment self-describes the whole
// frame (statusId + expectedStreams) so the agent can reconstruct the frame
// even if the frame header is lost.
class SensorData extends FieldsChunk
{
    string statusId;
    string actorId;
    uint64_t streamId;
    uint64_t fragmentNum;
    uint64_t totalFragments;
    uint64_t expectedStreams[];  // streamIds sampled in this frame

    string sensorType;
    simtime_t collectionTime;
}
