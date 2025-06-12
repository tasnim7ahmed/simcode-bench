#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaLanUdpExample");

int main(int argc, char *argv[])
{
    bool useIpv6 = false;
    bool enableLogging = false;

    CommandLine cmd;
    cmd.AddValue("useIpv6", "Enable IPv6 addressing (default: false/IPv4)", useIpv6);
    cmd.AddValue("enableLogging", "Enable verbose logging (default: false)", enableLogging);
    cmd.Parse(argc, argv);

    if (enableLogging)
    {
        LogComponentEnable("CsmaLanUdpExample", LOG_LEVEL_INFO);
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper internet;
    if (useIpv6)
    {
        internet.SetIpv4StackInstall(false);
    }
    internet.Install(nodes);

    Ipv4Address serverAddress4;
    Ipv6Address serverAddress6;

    if (!useIpv6)
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
        serverAddress4 = interfaces.GetAddress(1);
    }
    else
    {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:0:0:1::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer interfaces6 = ipv6.Assign(devices);
        // Ensure interfaces are set to up for IPv6
        interfaces6.SetForwarding(0, true);
        interfaces6.SetForwarding(1, true);
        interfaces6.SetDefaultRouteInAllNodes(0);
        serverAddress6 = interfaces6.GetAddress(1,1); // 1=second node, 1=global address
    }

    // UDP server (on n1)
    uint16_t port = 9000;
    ApplicationContainer serverApp;
    if (!useIpv6)
    {
        UdpServerHelper server(port);
        serverApp = server.Install(nodes.Get(1));
    }
    else
    {
        UdpServerHelper server(port);
        serverApp = server.Install(nodes.Get(1));
    }
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP client (on n0)
    ApplicationContainer clientApp;
    uint32_t maxPackets = 320;
    Time interval = MilliSeconds(50);
    uint32_t packetSize = 1024;

    if (!useIpv6)
    {
        UdpClientHelper client(serverAddress4, port);
        client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        client.SetAttribute("Interval", TimeValue(interval));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        clientApp = client.Install(nodes.Get(0));
    }
    else
    {
        UdpClientHelper client(serverAddress6, port);
        client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        client.SetAttribute("Interval", TimeValue(interval));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        clientApp = client.Install(nodes.Get(0));
    }
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}