#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool enableIpv6 = false;
    bool enableLogging = false;
    std::string traceFile = "trace.txt";

    CommandLine cmd;
    cmd.AddValue("enableIpv6", "Enable IPv6 addressing", enableIpv6);
    cmd.AddValue("enableLogging", "Enable logging", enableLogging);
    cmd.Parse(argc, argv);

    if (enableLogging) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer devices;
    devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv6AddressHelper address6;
    if (enableIpv6) {
        address6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer interfaces6 = address6.Assign(devices);
    } else {
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
    }

    if (!enableIpv6) {
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    } else {
        Ipv6GlobalRoutingHelper::PopulateRoutingTables();
    }

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer apps = server.Install(nodes.Get(1));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    UdpClientHelper client(enableIpv6 ? interfaces6.GetAddress(1) : interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("TraceFile", StringValue(traceFile));
    apps = client.Install(nodes.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}