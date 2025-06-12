#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/trace-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool verbose = false;
    bool useIpv6 = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("useIpv6", "Set to true to use IPv6 addressing.", useIpv6);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4InterfaceContainer interfaces;
    Ipv6InterfaceContainer ipv6Interfaces;

    if (useIpv6) {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
        ipv6Interfaces = ipv6.Assign(devices);

        Ptr<Ipv6StaticRoutingHelper> staticRouting = CreateObject<Ipv6StaticRoutingHelper>();
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Node> node = nodes.Get(i);
            Ptr<Ipv6RoutingTable> routingTable = node->GetObject<Ipv6RoutingTable>();
            routingTable->SetDefaultRoute(staticRouting->DefaultRoute());
        }

    } else {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    }

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(useIpv6 ? ipv6Interfaces.GetAddress(1) : interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    std::string traceFile = "scratch/tracefile.txt";
    client.SetAttribute("TraceFile", StringValue(traceFile));
    client.SetAttribute("UseTraceFile", BooleanValue(true));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}