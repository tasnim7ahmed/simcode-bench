#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/ipv6-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTraceClientServerSimulation");

int main(int argc, char *argv[]) {
    bool enableLogging = false;
    bool useIpv6 = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableLogging", "Enable logging for simulation events", enableLogging);
    cmd.AddValue("useIpv6", "Use IPv6 addressing instead of IPv4", useIpv6);
    cmd.Parse(argc, argv);

    if (enableLogging) {
        LogComponentEnable("UdpTraceClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer devices = csma.Install(nodes);

    if (useIpv6) {
        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv6AddressHelper address;
        address.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 9;

        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpTraceClientHelper echoClient(interfaces.GetAddress(1, 1), port, "src/examples/tutorial/first.trace");
        echoClient.SetAttribute("MaxPackets", UintegerValue(0)); // 0 means unlimited until EOF

        ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    } else {
        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 9;

        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpTraceClientHelper echoClient(interfaces.GetAddress(1), port, "src/examples/tutorial/first.trace");
        echoClient.SetAttribute("MaxPackets", UintegerValue(0)); // 0 means unlimited until EOF

        ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}