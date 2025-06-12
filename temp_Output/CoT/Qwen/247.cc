#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleIpAssignment");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("SimpleIpAssignment", LOG_LEVEL_INFO);

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
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    for (uint32_t i = 0; i < interfaces.GetN(); ++i) {
        Ptr<Ipv4> ipv4 = interfaces.Get(i)->first->GetObject<Ipv4>();
        uint32_t interfaceIndex = interfaces.Get(i)->second;
        Ipv4Address ipAddr = ipv4->GetAddress(interfaceIndex, 0).GetLocal();
        NS_LOG_INFO("Device " << i << " assigned IP address: " << ipAddr);
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}