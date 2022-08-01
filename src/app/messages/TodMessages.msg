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


import inet.common.INETDefs;
import inet.common.Units;
import inet.common.packet.chunk.Chunk;


namespace inet;

enum TODMessageType
{
   STATUS = 0;
   INSTRUCTION = 1;
};



class TODMessage extends FieldsChunk{
	//simtime_t creationTime;
	TODMessageType messageType;
}


class TodStatusUpdateMessage extends TODMessage{
    messageType = STATUS;
	string actorId;
	string statusId;
} 

class TodInstructionMessage extends TODMessage{
    messageType = INSTRUCTION;
    simtime_t statusCrationTime;
	string actorId;
	string instructionId;
}


