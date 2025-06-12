#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTraceClientServerExample");

int main(int argc, char *argv[]) {
    bool enableLogging = false;
    bool useIpv6 = false;
    std::string traceFile = "csma-udp-trace.txt";

    CommandLine cmd(__FILE__);
    cmd.AddValue("logging", "Enable logging", enableLogging);
    cmd.AddValue("ipv6", "Use IPv6 instead of IPv4", useIpv6);
    cmd.AddValue("traceFile", "Trace file name", traceFile);
    cmd.Parse(argc, argv);

    if (enableLogging) {
        LogComponentEnable("UdpTraceClient", LOG_LEVEL_ALL);
        LogComponentEnable("UdpServer", LOG_LEVEL_ALL);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4InterfaceContainer ipv4Interfaces;
    Ipv6InterfaceContainer ipv6Interfaces;

    if (useIpv6) {
        stack.EnableIpv6();
        Ipv6AddressHelper address;
        address.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
        ipv6Interfaces = address.Assign(devices);
    } else {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        ipv4Interfaces = address.Assign(devices);
    }

    uint16_t port = 4000;

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpTraceClientHelper client;
    if (useIpv6) {
        client = UdpTraceClientHelper(ipv6Interfaces.GetAddress(1, 1), port, traceFile);
    } else {
        client = UdpTraceClientHelper(ipv4Interfaces.GetAddress(1), port, traceFile);
    }
    client.SetAttribute("MaxPacketSize", UintegerValue(1472));
    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}