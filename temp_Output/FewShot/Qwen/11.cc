#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool enableLogging = false;
    bool useIpv6 = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("logging", "Enable logging for UDP client and server.", enableLogging);
    cmd.AddValue("ipv6", "Use IPv6 addressing instead of IPv4.", useIpv6);
    cmd.Parse(argc, argv);

    if (enableLogging) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv6AddressHelper ipv6;
    ipv6.SetBase("2001:db::", Ipv6Prefix(64));

    Ipv4InterfaceContainer ipv4Interfaces;
    Ipv6InterfaceContainer ipv6Interfaces;

    if (useIpv6) {
        ipv6Interfaces = ipv6.Assign(devices);
    } else {
        ipv4Interfaces = ipv4.Assign(devices);
    }

    uint16_t port = 4000;

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    Address serverAddress;
    if (useIpv6) {
        serverAddress = Inet6SocketAddress(ipv6Interfaces.GetAddress(1, 1), port);
    } else {
        serverAddress = InetSocketAddress(ipv4Interfaces.GetAddress(1), port);
    }

    UdpEchoClientHelper echoClient(serverAddress);
    echoClient.SetAttribute("MaxPackets", UintegerValue(320));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}