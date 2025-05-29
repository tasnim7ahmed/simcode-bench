#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleCsmaLanUdpExample");

int main(int argc, char *argv[])
{
    bool useIpv6 = false;
    bool enableLogging = false;

    CommandLine cmd;
    cmd.AddValue("useIpv6", "Enable IPv6 addressing (default: false)", useIpv6);
    cmd.AddValue("enableLogging", "Enable logging for applications and CSMA (default: false)", enableLogging);
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
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4Address serverAddress4;
    Ipv6Address serverAddress6;
    uint16_t port = 4000;

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
        ipv6.SetBase("2001:1::", 64);
        Ipv6InterfaceContainer interfaces6 = ipv6.Assign(devices);
        interfaces6.SetForwarding(0, true);
        interfaces6.SetForwarding(1, true);
        interfaces6.SetDefaultRouteInAllNodes(0);
        serverAddress6 = interfaces6.GetAddress(1, 1);
    }

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    uint32_t maxPackets = 320;
    Time interPacketInterval = MilliSeconds(50);
    uint32_t packetSize = 1024;

    UdpClientHelper client(
        (!useIpv6) ? Address(serverAddress4) : Address(serverAddress6),
        port);
    client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}