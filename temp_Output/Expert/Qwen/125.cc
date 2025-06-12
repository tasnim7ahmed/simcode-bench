#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-rip.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipStarTopology");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("RipStarTopology", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4Rip", LOG_LEVEL_ALL);

    NodeContainer nodes;
    nodes.Create(5);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

    NetDeviceContainer centralDevices;
    NetDeviceContainer edgeDevices[4];

    for (uint32_t i = 0; i < 4; ++i) {
        NetDeviceContainer devicePair = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(i + 1)));
        centralDevices.Add(devicePair.Get(0));
        edgeDevices[i].Add(devicePair.Get(1));
    }

    InternetStackHelper stack;
    stack.SetRoutingHelper(Ipv4RipHelper()); // Use RIP for dynamic routing
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[5];

    address.SetBase("10.0.0.0", "255.255.255.0");
    interfaces[0] = address.Assign(centralDevices);
    address.NewNetwork();
    for (uint32_t i = 0; i < 4; ++i) {
        interfaces[i + 1] = address.Assign(edgeDevices[i]);
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> routingStream = ascii.CreateFileStream("rip.routes");
    Ipv4RipHelper::PrintRoutingTableAllAt(Seconds(2.0), routingStream);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}