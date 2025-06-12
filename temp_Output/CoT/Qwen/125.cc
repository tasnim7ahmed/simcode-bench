#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/rip-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipStarTopology");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("RipStarTopology", LOG_LEVEL_INFO);
    LogComponentEnable("Rip", LOG_LEVEL_ALL);

    // Create nodes
    NodeContainer centralRouter;
    centralRouter.Create(1);

    NodeContainer edgeRouters;
    edgeRouters.Create(4);

    // Connect each edge router to the central router via point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer centralDevices;
    NetDeviceContainer edgeDevices[4];

    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer centralInterfaces;
    Ipv4InterfaceContainer edgeInterfaces[4];

    for (uint32_t i = 0; i < 4; ++i) {
        ipv4.SetBase("10.1." + std::to_string(i) + ".0", "255.255.255.0");
        NetDeviceContainer devices = p2p.Install(centralRouter.Get(0), edgeRouters.Get(i));
        centralDevices.Add(devices.Get(0));
        edgeDevices[i].Add(devices.Get(1));

        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
        centralInterfaces.Add(interfaces.Get(0));
        edgeInterfaces[i].Add(interfaces.Get(1));
    }

    // Install Internet stack with RIP routing protocol
    RipHelper ripRouting;

    InternetStackHelper internetCentral;
    internetCentral.SetRoutingHelper(ripRouting);
    internetCentral.Install(centralRouter);

    InternetStackHelper internetEdge;
    internetEdge.SetRoutingHelper(ripRouting);
    internetEdge.Install(edgeRouters);

    // Assign IP addresses to all nodes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable routing table tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> routingStream = asciiTraceHelper.CreateFileStream("rip-routing-tables.tr");
    ripRouting.PrintRoutingTableAllAt(Seconds(10.0), routingStream);

    // Simulation duration
    Simulator::Stop(Seconds(15.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}