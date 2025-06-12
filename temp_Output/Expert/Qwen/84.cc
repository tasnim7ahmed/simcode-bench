#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpTraceFileSimulation");

int main(int argc, char *argv[]) {
    bool enableLogging = false;
    bool useIpv6 = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("enable-logging", "Enable logging output", enableLogging);
    cmd.AddValue("use-ipv6", "Use IPv6 addressing instead of IPv4", useIpv6);
    cmd.Parse(argc, argv);

    if (enableLogging) {
        LogComponentEnable("CsmaUdpTraceFileSimulation", LOG_LEVEL_INFO);
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
        stack.SetIpv4StackEnabled(false);
        stack.SetIpv6StackEnabled(true);
        stack.Install(nodes);
    } else {
        stack.Install(nodes);
    }

    Ipv4AddressHelper ipv4;
    Ipv6AddressHelper ipv6;
    Ipv4InterfaceContainer ipv4Interfaces;
    Ipv6InterfaceContainer ipv6Interfaces;

    if (useIpv6) {
        ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
        ipv6Interfaces = ipv6.Assign(devices);
    } else {
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4Interfaces = ipv4.Assign(devices);
    }

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    std::string traceFilePath = "csma-udp-packet-trace.tr";
    UdpClientTraceHelper client(useIpv6 ? ipv6Interfaces.GetAddress(1, 1) : ipv4Interfaces.GetAddress(1), port, traceFilePath);
    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}