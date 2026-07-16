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

// Datas request from sensorManager towards a sensor
class SensorDataRequest extends FieldsChunk
{
    uint64_t frameId;
    simtime_t requestTime;
}

// Response from sensor to sensorManager
class SensorDataResponse extends FieldsChunk
{
    uint64_t streamId;
    uint64_t frameId;
    uint64_t fragmentNum;
    uint64_t totalFragments;

    string sensorType;
    simtime_t collectionTime;
}
