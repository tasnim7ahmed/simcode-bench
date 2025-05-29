#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    bool useIpv6 = false;
    bool enableLogging = false;

    CommandLine cmd;
    cmd.AddValue("ipv6", "Enable IPv6 addressing if true, else use IPv4", useIpv6);
    cmd.AddValue("log", "Enable verbose logging", enableLogging);
    cmd.Parse(argc, argv);

    if (enableLogging)
    {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("CsmaNetDevice", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4Address serverAddress4 = "10.1.1.2";
    Ipv6Address serverAddress6 = Ipv6Address("2001:1::2");

    uint16_t port = 9000;

    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    if (useIpv6)
    {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer ifaces = ipv6.Assign(devices);
        // Bring up all interfaces
        ifaces.SetForwarding(0, true);
        ifaces.SetDefaultRouteInAllNodes(0);

        UdpServerHelper server(port);
        serverApps = server.Install(nodes.Get(1));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(10.0));

        UdpClientHelper client(ifaces.GetAddress(1, 1), port);
        client.SetAttribute("MaxPackets", UintegerValue(320));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        clientApps = client.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }
    else
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ifaces = ipv4.Assign(devices);

        UdpServerHelper server(port);
        serverApps = server.Install(nodes.Get(1));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(10.0));

        UdpClientHelper client(ifaces.GetAddress(1), port);
        client.SetAttribute("MaxPackets", UintegerValue(320));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        clientApps = client.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}