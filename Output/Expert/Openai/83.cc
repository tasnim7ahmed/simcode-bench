#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    bool useIpv6 = false;
    bool enableLogging = false;

    CommandLine cmd;
    cmd.AddValue("ipv6", "Enable IPv6 (default: false, use IPv4)", useIpv6);
    cmd.AddValue("logging", "Enable logging (default: false)", enableLogging);
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
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0.1)));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4Address serverIpv4 = "10.1.1.2";
    Ipv6Address serverIpv6 = "2001:db8::2";
    uint16_t port = 9;

    Ipv4Address clientIpv4;
    Ipv6Address clientIpv6;

    if (!useIpv6)
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ifs = ipv4.Assign(devices);
        clientIpv4 = ifs.GetAddress(0);
        serverIpv4 = ifs.GetAddress(1);
    }
    else
    {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase("2001:db8::", 64);
        Ipv6InterfaceContainer ifs6 = ipv6.Assign(devices);
        ifs6.SetForwarding(0, true);
        ifs6.SetDefaultRouteInAllNodes(0);
        clientIpv6 = ifs6.GetAddress(0, 1);
        serverIpv6 = ifs6.GetAddress(1, 1);
    }

    UdpServerHelper serverHelper(port);
    ApplicationContainer serverApp = serverHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    uint32_t maxPackets = 320;
    Time interPacketInterval = MilliSeconds(50);
    uint32_t packetSize = 1024;

    ApplicationContainer clientApp;

    if (!useIpv6)
    {
        UdpClientHelper clientHelper(serverIpv4, port);
        clientHelper.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        clientHelper.SetAttribute("Interval", TimeValue(interPacketInterval));
        clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        clientApp = clientHelper.Install(nodes.Get(0));
    }
    else
    {
        UdpClientHelper clientHelper(serverIpv6, port);
        clientHelper.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        clientHelper.SetAttribute("Interval", TimeValue(interPacketInterval));
        clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        clientApp = clientHelper.Install(nodes.Get(0));
    }
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}