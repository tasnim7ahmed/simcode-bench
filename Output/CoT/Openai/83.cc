#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleLanUdpFlow");

int main(int argc, char *argv[])
{
    bool enableIpv6 = false;
    bool enableLogging = false;

    CommandLine cmd;
    cmd.AddValue("Ipv6", "Enable IPv6 addressing (default: IPv4)", enableIpv6);
    cmd.AddValue("Logging", "Enable logging", enableLogging);
    cmd.Parse(argc, argv);

    if (enableLogging)
    {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("SimpleLanUdpFlow", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    if (!enableIpv6)
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ifacesv4 = ipv4.Assign(devices);
        uint16_t port = 9;

        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(nodes.Get(1));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(10.0));

        UdpClientHelper client(ifacesv4.GetAddress(1), port);
        client.SetAttribute("MaxPackets", UintegerValue(320));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = client.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }
    else
    {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer ifacesv6 = ipv6.Assign(devices);
        ifacesv6.SetForwarding(0, true);
        ifacesv6.SetForwarding(1, true);
        ifacesv6.SetDefaultRouteInAllNodes(0);

        uint16_t port = 9;

        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(nodes.Get(1));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(10.0));

        UdpClientHelper client(ifacesv6.GetAddress(1,1), port);
        client.SetAttribute("MaxPackets", UintegerValue(320));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = client.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}