#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h"

using namespace ns3;

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool ipv6 = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("ipv6", "Set to true to enable IPv6", ipv6);

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

    Ipv4AddressHelper address;
    Ipv6AddressHelper address6;
    Ipv4InterfaceContainer interfaces;
    Ipv6InterfaceContainer interfaces6;

    if (ipv6) {
        address6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
        interfaces6 = address6.Assign(devices);

    } else {
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);
    }

    uint16_t port = 4000;

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    uint32_t maxPacketSize = 1472;

    UdpClientHelper client(ipv6 ? interfaces6.GetAddress(1, 1) : interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
    client.SetAttribute("PacketSize", UintegerValue(maxPacketSize));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    if (!ipv6) {
       Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    } else {
       Ipv6GlobalRoutingHelper::PopulateRoutingTables();
    }

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();

    Simulator::Destroy();

    return 0;
}