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
    bool verbose = true;
    uint32_t printRoutingTables = 1;
    uint32_t linkDown = 50;
    uint32_t linkUp = 60;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "turn on log components", verbose);
    cmd.AddValue("printRoutingTableEvery", "Interval (seconds) between routing table dumps.", printRoutingTables);
    cmd.AddValue("linkDown", "Time (seconds) to bring central link down.", linkDown);
    cmd.AddValue("linkUp", "Time (seconds) to bring central link back up.", linkUp);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("RipStarTopology", LOG_LEVEL_INFO);
        LogComponentEnable("Rip", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv4Interface", LOG_LEVEL_ALL);
    }

    // Create nodes
    NodeContainer centralRouter;
    centralRouter.Create(1);

    NodeContainer edgeRouters;
    edgeRouters.Create(4);

    NodeContainer allNodes;
    allNodes.Add(centralRouter);
    allNodes.Add(edgeRouters);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    Ipv4AddressHelper ipv4;
    NetDeviceContainer centralDevices[4], edgeDevices[4];

    for (uint32_t i = 0; i < 4; ++i) {
        ipv4.SetBase("10.0." + std::to_string(i) + ".0", "255.255.255.0");
        NetDeviceContainer devices = p2p.Install(NodeContainer(centralRouter.Get(0), edgeRouters.Get(i)));
        centralDevices[i] = devices.Get(0);
        edgeDevices[i] = devices.Get(1);
    }

    InternetStackHelper internet;
    RipHelper rip;

    // Install RIP on all nodes
    internet.SetRoutingHelper(rip);
    internet.Install(allNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer centralInterfaces[4], edgeInterfaces[4];
    for (uint32_t i = 0; i < 4; ++i) {
        ipv4.Assign(centralDevices[i]);
        ipv4.Assign(edgeDevices[i]);
        centralInterfaces[i] = internet.GetIpv4(centralRouter.Get(0))->GetInterfaceForDevice(centralDevices[i]);
        edgeInterfaces[i] = internet.GetIpv4(edgeRouters.Get(i))->GetInterfaceForDevice(edgeDevices[i]);
    }

    // Set metrics to influence route selection
    rip.ExcludeInterface(centralRouter.Get(0), 1); // Loopback is interface 0

    // Enable periodic updates and garbage collection
    rip.SetInterfaceMetric(centralRouter.Get(0), centralInterfaces[0].GetInterfaceIndex(), 1);
    for (uint32_t i = 0; i < 4; ++i) {
        rip.SetInterfaceMetric(edgeRouters.Get(i), edgeInterfaces[i].GetInterfaceIndex(), 1);
    }

    // Trace routing tables
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> routingStream = ascii.CreateFileStream("rip-star.routes");
    rip.PrintRoutingTableAt(Seconds(printRoutingTables), centralRouter.Get(0), routingStream);
    for (uint32_t i = 0; i < 4; ++i) {
        rip.PrintRoutingTableAt(Seconds(printRoutingTables), edgeRouters.Get(i), routingStream);
    }

    // Bring one link down and then back up
    Ipv4Address centralIp = internet.GetIpv4(centralRouter.Get(0))->GetAddress(centralInterfaces[0].GetInterfaceIndex(), 0).GetLocal();
    Simulator::Schedule(Seconds(linkDown), &Ipv4::SetDown, internet.GetIpv4(centralRouter.Get(0)), centralInterfaces[0].GetInterfaceIndex());
    Simulator::Schedule(Seconds(linkUp), &Ipv4::SetUp, internet.GetIpv4(centralRouter.Get(0)), centralInterfaces[0].GetInterfaceIndex());

    Simulator::Stop(Seconds(100));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}