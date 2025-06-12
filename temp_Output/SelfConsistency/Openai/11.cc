#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpExample");

int
main(int argc, char *argv[])
{
    bool useIpv6 = false;
    bool enableLogging = false;

    CommandLine cmd;
    cmd.AddValue("ipv6", "Use IPv6 (otherwise, use IPv4)", useIpv6);
    cmd.AddValue("logging", "Enable UDP client/server logging", enableLogging);
    cmd.Parse(argc, argv);

    if (enableLogging)
    {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("CsmaUdpExample", LOG_LEVEL_INFO);
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    if (useIpv6)
    {
        Ipv6AddressHelper ipv6;
        stack.Install(nodes);

        ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

        // UDP server on n1
        uint16_t port = 4000;
        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(nodes.Get(1));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        // UDP client on n0
        UdpClientHelper client(interfaces.GetAddress(1, 1), port);
        client.SetAttribute("MaxPackets", UintegerValue(320));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = client.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

    }
    else // IPv4
    {
        Ipv4AddressHelper ipv4;
        stack.Install(nodes);

        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        // UDP server on n1
        uint16_t port = 4000;
        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(nodes.Get(1));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        // UDP client on n0
        UdpClientHelper client(interfaces.GetAddress(1), port);
        client.SetAttribute("MaxPackets", UintegerValue(320));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = client.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }

    NS_LOG_INFO("Running simulation...");
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Simulation complete.");

    return 0;
}