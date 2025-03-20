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

#ifndef __TOD_OMNET_IPV4NODECONFIGURATOREXTENDED_H_
#define __TOD_OMNET_IPV4NODECONFIGURATOREXTENDED_H_

#include <omnetpp.h>
#include "inet/networklayer/configurator/ipv4/Ipv4NodeConfigurator.h"

#include "inet/common/ModuleRefByPar.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/lifecycle/OperationalBase.h"

using namespace omnetpp;
using namespace inet;


class Ipv4NodeConfiguratorExtended : virtual public Ipv4NodeConfigurator
{
    protected:
        virtual void handleStartOperation(LifecycleOperation *operation);
        virtual bool handleOperationStage(LifecycleOperation *operation, IDoneCallback *doneCallback) override;
};

#endif
