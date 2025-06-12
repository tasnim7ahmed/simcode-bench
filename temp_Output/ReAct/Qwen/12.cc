#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTraceClientServerSimulation");

int main(int argc, char *argv[]) {
    bool enableLogging = false;
    bool useIpv6 = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("logging", "Enable detailed logging", enableLogging);
    cmd.AddValue("ipv6", "Use IPv6 addressing instead of IPv4", useIpv6);
    cmd.Parse(argc, argv);

    if (enableLogging) {
        LogComponentEnable("UdpTraceClientServerSimulation", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServerApplication", LOG_LEVEL_ALL);
        LogComponentEnable("UdpTraceClientApplication", LOG_LEVEL_ALL);
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

    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer interfaces;
    if (!useIpv6) {
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = ipv4.Assign(devices);
    }

    Ipv6AddressHelper ipv6;
    if (useIpv6) {
        ipv6.SetBase("2001:db8::", Ipv6Prefix(64));
        Ipv6InterfaceContainer ipv6Interfaces = ipv6.Assign(devices);
    }

    uint16_t port = 4000;

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    std::string filename = "udp-trace-file.trace";
    UdpTraceClientHelper client;
    if (!useIpv6) {
        client = UdpTraceClientHelper(interfaces.GetAddress(1), port, filename);
    } else {
        client = UdpTraceClientHelper(nodes.Get(1)->GetObject<Ipv6>()->GetAddress(1, 1).GetAddress(), port, filename);
    }

    client.SetAttribute("MaxPacketSize", UintegerValue(1472));
    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}