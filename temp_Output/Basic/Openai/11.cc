#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpSimulation");

int main(int argc, char *argv[])
{
    bool useIpv6 = false;
    bool enableLogging = false;

    CommandLine cmd;
    cmd.AddValue("useIpv6", "Use IPv6 addressing if true; otherwise, use IPv4", useIpv6);
    cmd.AddValue("enableLogging", "Enable UDP client/server logging", enableLogging);
    cmd.Parse(argc, argv);

    if (enableLogging)
    {
        LogComponentEnable("CsmaUdpSimulation", LOG_LEVEL_INFO);
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(2);

    NS_LOG_INFO("Configuring CSMA channel.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4Address serverIpv4 ("10.1.1.2");
    Ipv6Address serverIpv6 ("2001:db8::2");
    Ipv4InterfaceContainer ipv4Ifaces;
    Ipv6InterfaceContainer ipv6Ifaces;

    if (!useIpv6)
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4Ifaces = ipv4.Assign(devices);
    }
    else
    {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
        ipv6Ifaces = ipv6.Assign(devices);
        ipv6.NewNetwork();
    }

    uint16_t port = 4000;

    NS_LOG_INFO("Setting up UDP server.");
    Address serverAddress;
    Ptr<UdpServer> serverApp;
    ApplicationContainer serverApps;
    if (!useIpv6)
    {
        serverAddress = InetSocketAddress(ipv4Ifaces.GetAddress(1), port);
        UdpServerHelper server(port);
        serverApps = server.Install(nodes.Get(1));
    }
    else
    {
        serverAddress = Inet6SocketAddress(ipv6Ifaces.GetAddress(1,1), port);
        UdpServerHelper server(port);
        server.SetAttribute("Protocol", TypeIdValue(UdpSocketFactory::GetTypeId()));
        serverApps = server.Install(nodes.Get(1));
    }
    serverApp = DynamicCast<UdpServer>(serverApps.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    NS_LOG_INFO("Setting up UDP client.");
    uint32_t maxPackets = 320;
    Time interPacketInterval = MilliSeconds(50);
    uint32_t packetSize = 1024;
    UdpClientHelper clientHelper(serverAddress);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    clientHelper.SetAttribute("Interval", TimeValue(interPacketInterval));
    clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    if (enableLogging)
    {
        NS_LOG_INFO("UDP simulation started with logging enabled.");
        if (useIpv6)
        {
            NS_LOG_INFO("IPv6 Mode. Server address: " << ipv6Ifaces.GetAddress(1,1));
        }
        else
        {
            NS_LOG_INFO("IPv4 Mode. Server address: " << ipv4Ifaces.GetAddress(1));
        }
        NS_LOG_INFO("CSMA: DataRate=5Mbps, Delay=2ms, MTU=1400");
        NS_LOG_INFO("UDP Client on n0 -> UDP Server on n1, port 4000");
    }
    else
    {
        NS_LOG_INFO("UDP simulation started.");
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    if (enableLogging)
    {
        uint32_t totalRx = serverApp->GetReceived();
        NS_LOG_INFO("UDP Server received " << totalRx << " packets.");
    }

    Simulator::Destroy();
    return 0;
}