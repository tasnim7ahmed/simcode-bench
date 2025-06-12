#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTraceFileSimulation");

int main(int argc, char *argv[]) {
    bool enableLogging = false;
    bool useIpv6 = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("logging", "Enable detailed logging", enableLogging);
    cmd.AddValue("ipv6", "Use IPv6 addressing instead of IPv4", useIpv6);
    cmd.Parse(argc, argv);

    if (enableLogging) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ALL);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ALL);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    if (useIpv6) {
        stack.Install(nodes);
    } else {
        Ipv4GlobalRoutingHelper routing;
        stack.SetRoutingHelper(routing);
        stack.Install(nodes);
    }

    Ipv4AddressHelper ipv4;
    Ipv6AddressHelper ipv6;
    Ipv4InterfaceContainer interfaces;
    Ipv6InterfaceContainer interfaces6;

    if (!useIpv6) {
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = ipv4.Assign(devices);
    } else {
        ipv6.SetBase("2001:db8::", Ipv6Prefix(64));
        interfaces6 = ipv6.Assign(devices);
    }

    uint16_t port = 4000;

    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1));
    serverApps.Stop(Seconds(10));

    std::string filePath = "csma-udp-trace-file"; // This is just a placeholder; actual trace file path must be provided
    UdpTraceClientHelper client;
    if (!useIpv6) {
        client = UdpTraceClientHelper(interfaces.GetAddress(1), port, filePath);
    } else {
        client = UdpTraceClientHelper(interfaces6.GetAddress(1, 1), port, filePath);
    }
    client.SetAttribute("MaxPacketSize", UintegerValue(1472)); // 1500 - 28 bytes for IP and UDP headers

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2));
    clientApps.Stop(Seconds(10));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}