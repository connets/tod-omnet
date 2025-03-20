#include "MultiNicHostAutoConfigurator.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/contract/ipv4/Ipv4Address.h"
#include "inet/networklayer/ipv4/Ipv4InterfaceData.h"

using namespace inet;

Define_Module(MultiNicHostAutoConfigurator);

/**
 * Override del metodo di assegnazione degli ip di HostAutoConfigurator.
 * Assegnamo per ciascuna nic un ip diverso
 */
void MultiNicHostAutoConfigurator::setupNetworkLayer() {
    EV_INFO << "host auto configuration started" << std::endl;

    std::string addressesBase = par("addressesBase");
    std::string interfaces = par("interfaces");
    Ipv4Address netmask = Ipv4Address(par("netmask").stringValue());
    std::string mcastGroups = par("mcastGroups").stdstringValue();
    std::string applyMcastGroupsTo = par("applyMcastGroupsTo");

    // get our host module
    cModule *host = getContainingNode(this);

    // get our routing table
    IIpv4RoutingTable *routingTable = L3AddressResolver().getIpv4RoutingTableOf(
            host);
    if (!routingTable)
        throw cRuntimeError("No routing table found");

    // look at all interface table entries
    cStringTokenizer interfaceTokenizer(interfaces.c_str());

    // look at all base addresses
    cStringTokenizer addressBaseTokenizer(addressesBase.c_str());

    const char *ifname;
    uint32_t loopbackAddr = Ipv4Address::LOOPBACK_ADDRESS.getInt();

    while ((ifname = interfaceTokenizer.nextToken()) != nullptr) {
        NetworkInterface *ie = interfaceTable->findInterfaceByName(ifname);
        if (!ie)
            throw cRuntimeError("No such interface '%s'", ifname);

        auto ipv4Data = ie->getProtocolDataForUpdate<Ipv4InterfaceData>();
        // assign IP Address to all connected interfaces
        if (ie->isLoopback()) {
            ipv4Data->setIPAddress(Ipv4Address(loopbackAddr++));
            ipv4Data->setNetmask(Ipv4Address::LOOPBACK_NETMASK);
            ipv4Data->setMetric(1);
            EV_INFO << "loopback interface " << ifname << " gets "
                           << ipv4Data->getIPAddress() << "/"
                           << ipv4Data->getNetmask() << std::endl;
            continue;
        }

        //per ogni interfaccia generiamo un ip su sottorete diversa
        Ipv4Address addressBase = Ipv4Address(addressBaseTokenizer.nextToken());
        Ipv4Address myAddress = Ipv4Address(
                addressBase.getInt() + uint32_t(host->getId()));

        // address test
        if (!Ipv4Address::maskedAddrAreEqual(myAddress, addressBase, netmask))
            throw cRuntimeError(
                    "Generated IP address is out of specified address range");

        EV_INFO << "interface " << ifname << " gets " << myAddress.str() << "/"
                       << netmask.str() << std::endl;

        ipv4Data->setIPAddress(myAddress);
        ipv4Data->setNetmask(netmask);
        ie->setBroadcast(true);

        // associate interface with default multicast groups
        ipv4Data->joinMulticastGroup(Ipv4Address::ALL_HOSTS_MCAST);
        ipv4Data->joinMulticastGroup(Ipv4Address::ALL_ROUTERS_MCAST);

        if (applyMcastGroupsTo.find(std::string(ifname)) != std::string::npos) {
            // associate specified interface with specified multicast groups
            cStringTokenizer interfaceTokenizer(mcastGroups.c_str());
            const char *mcastGroup_s;
            while ((mcastGroup_s = interfaceTokenizer.nextToken()) != nullptr) {
                Ipv4Address mcastGroup(mcastGroup_s);
                ipv4Data->joinMulticastGroup(mcastGroup);
            }
        }
    }
}
