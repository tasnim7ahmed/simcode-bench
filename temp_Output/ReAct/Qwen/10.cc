#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoExample");

int main(int argc, char *argv[]) {
    bool enableLogging = true;
    bool useIpv6 = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("logging", "Enable logging", enableLogging);
    cmd.AddValue("useIpv6", "Use IPv6 instead of IPv4", useIpv6);
    cmd.Parse(argc, argv);

    if (enableLogging) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

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
    Ipv4InterfaceContainer interfaces;
    if (!useIpv6) {
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = ipv4.Assign(devices);
    }

    Ipv6AddressHelper ipv6;
    Ipv6InterfaceContainer ipv6Interfaces;
    if (useIpv6) {
        ipv6.SetBase("2001:db8::", Ipv6Prefix(64));
        ipv6Interfaces = ipv6.Assign(devices);
    }

    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(11.0));

    UdpEchoClientHelper client;
    if (useIpv6) {
        client = UdpEchoClientHelper(ipv6Interfaces.GetAddress(1, 1), port);
    } else {
        client = UdpEchoClientHelper(interfaces.GetAddress(1), port);
    }
    client.SetAttribute("MaxPackets", UintegerValue(9));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("udp-echo.tr"));
    csma.EnablePcapAll("udp-echo");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}