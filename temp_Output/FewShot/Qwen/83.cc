#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool enableIpv6 = false;
    bool enableLogging = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableIpv6", "Enable IPv6 addressing instead of IPv4", enableIpv6);
    cmd.AddValue("enableLogging", "Enable logging for applications", enableLogging);
    cmd.Parse(argc, argv);

    if (enableLogging) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4InterfaceContainer interfacesv4;
    Ipv6InterfaceContainer interfacesv6;

    if (enableIpv6) {
        interfacesv6 = Ipv6AddressHelper().Assign(devices);
        interfacesv6.SetForwarding(1, true);
    } else {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfacesv4 = address.Assign(devices);
    }

    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    Address serverAddress;
    if (enableIpv6) {
        serverAddress = Inet6SocketAddress(interfacesv6.GetAddress(1, 1), port);
    } else {
        serverAddress = InetSocketAddress(interfacesv4.GetAddress(1), port);
    }

    UdpEchoClientHelper client(serverAddress);
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}