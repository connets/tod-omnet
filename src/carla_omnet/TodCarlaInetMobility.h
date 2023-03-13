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

#ifndef __TOD_OMNET_NETWORK_TODCARLAINETMOBILITY_H_
#define __TOD_OMNET_NETWORK_TODCARLAINETMOBILITY_H_


#include <omnetpp.h>
#include "carlanet/CarlaInetMobility.h"

using namespace omnetpp;


class TodCarlaInetMobility : public CarlaInetMobility
{
public:
    virtual void initialize(int stage) override;
};

#endif
