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

#include "TodCarlaInetMobility.h"

Define_Module(TodCarlaInetMobility);

void TodCarlaInetMobility::updateCarlaActorConfigurationFromParam(cValueMap *confMap)
{
    confMap->set("route", par("route").stdstringValue());
    confMap->set("configuration_id", par("configuration_id").stdstringValue());
    confMap->set("agent_configuration", par("agent_configuration").stdstringValue());
    // convention: using the module name as agent id
    confMap->set("agent_id", getParentModule()->getFullName());
}

