#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    bool useIpv6 = false;
    bool enableLogs = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("useIpv6", "Use IPv6 addressing instead of IPv4", useIpv6);
    cmd.AddValue("enableLogs", "Enable detailed logging for UDP client/server", enableLogs);
    cmd.Parse(argc, argv);

    if (enableLogs)
    {
        LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2); 

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4InterfaceContainer ipv4Interfaces;
    Ipv6InterfaceContainer ipv6Interfaces;

    if (useIpv6)
    {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase("2001:db8::", Ipv6Prefix(64));
        ipv6Interfaces = ipv6.Assign(devices);
    }
    else
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4Interfaces = ipv4.Assign(devices);
    }

    uint16_t serverPort = 4000;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApps = udpServer.Install(nodes.Get(1)); 
    serverApps.Start(Seconds(0.0)); 
    serverApps.Stop(Seconds(10.0));

    Ptr<Node> clientNode = nodes.Get(0); 

    Address serverAddress;
    if (useIpv6)
    {
        serverAddress = Inet6SocketAddress(ipv6Interfaces.GetAddress(1, 1), serverPort); 
    }
    else
    {
        serverAddress = InetSocketAddress(ipv4Interfaces.GetAddress(1), serverPort); 
    }

    UdpClientHelper udpClient(serverAddress);
    udpClient.SetAttribute("MaxPacketSize", UintegerValue(1024));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    udpClient.SetAttribute("PacketCount", UintegerValue(320)); 

    ApplicationContainer clientApps = udpClient.Install(clientNode);
    clientApps.Start(Seconds(2.0)); 
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}