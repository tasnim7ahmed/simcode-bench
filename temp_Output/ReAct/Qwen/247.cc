#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleIpAssignment");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 4;

    NodeContainer nodes;
    nodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        devices.Add(p2p.Install(nodes.Get(i), nodes.Get(i + 1)));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces;
    for (uint32_t i = 0; i < devices.GetN(); ++i) {
        Ipv4InterfaceContainer ifc = address.Assign(devices.Get(i));
        interfaces.Add(ifc);
        address.NewNetwork();
    }

    for (uint32_t i = 0; i < interfaces.GetN(); ++i) {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        NS_ASSERT(ipv4 != nullptr);
        for (uint32_t j = 0; j < ipv4->GetNInterfaces(); ++j) {
            Ipv4Address addr = ipv4->GetAddress(j, 0).GetLocal();
            std::cout << "Node " << i << " interface " << j << " IP: " << addr << std::endl;
        }
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}