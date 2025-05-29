#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpFlowExample");

int main(int argc, char *argv[]) {
    bool useIpv6 = false;
    bool enableLogging = false;

    CommandLine cmd;
    cmd.AddValue("ipv6", "Enable IPv6 addressing (default: false, use IPv4)", useIpv6);
    cmd.AddValue("logging", "Enable logging (default: false)", enableLogging);
    cmd.Parse(argc, argv);

    if (enableLogging) {
        LogComponentEnable("CsmaUdpFlowExample", LOG_LEVEL_INFO);
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

    uint16_t port = 4000;
    ApplicationContainer serverApp, clientApp;

    if (!useIpv6) {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        UdpEchoServerHelper echoServer(port);
        serverApp = echoServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(320));
        echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        clientApp = echoClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    } else {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer interfaces6 = ipv6.Assign(devices);
        interfaces6.SetForwarding(0, true);
        interfaces6.SetForwarding(1, true);
        interfaces6.SetDefaultRouteInAllNodes(0);

        UdpEchoServerHelper echoServer(port);
        serverApp = echoServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(interfaces6.GetAddress(1,1), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(320));
        echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        clientApp = echoClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}