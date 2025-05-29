#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    std::string addressFamily = "Ipv4";
    bool enableLogging = false;

    CommandLine cmd;
    cmd.AddValue("addressFamily", "Addressing protocol (Ipv4 or Ipv6)", addressFamily);
    cmd.AddValue("enableLogging", "Enable UDP Logging (true|false)", enableLogging);
    cmd.Parse(argc, argv);

    if (enableLogging)
    {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    uint16_t port = 4000;
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    Ipv4Address serverIpv4;
    Ipv6Address serverIpv6;

    if (addressFamily == "Ipv4")
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
        serverIpv4 = interfaces.GetAddress(1);
        UdpServerHelper server(port);
        serverApps = server.Install(nodes.Get(1));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpClientHelper client(serverIpv4, port);
        client.SetAttribute("MaxPackets", UintegerValue(320));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        clientApps = client.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }
    else
    {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);
        interfaces.SetForwarding(0, true);
        interfaces.SetDefaultRouteInAllNodes(0);
        serverIpv6 = interfaces.GetAddress(1, 1); // index 1, global address
        UdpServerHelper server(port);
        serverApps = server.Install(nodes.Get(1));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));
        UdpClientHelper client(serverIpv6, port);
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