#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h"

using namespace ns3;

int main(int argc, char* argv[]) {
    bool useIpv6 = false;
    bool tracing = false;

    CommandLine cmd;
    cmd.AddValue("useIpv6", "Set to true to enable IPv6; otherwise, use IPv4", useIpv6);
    cmd.AddValue("tracing", "Set to true to enable tracing", tracing);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase("2001:db8::", Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces6;
    if (useIpv6) {
        interfaces6 = ipv6.Assign(devices);
        for (uint32_t i = 0; i < interfaces6.GetN(); ++i) {
            interfaces6.GetAddress(i, 1);
            nodes.Get(i)->GetObject<Ipv6>()->SetRouterAdvertisement(false);
        }
    }

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient;
    if (useIpv6) {
        echoClient = UdpEchoClientHelper(interfaces6.GetAddress(1, 1), port);
    } else {
        echoClient = UdpEchoClientHelper(interfaces.GetAddress(1), port);
    }
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    if (!useIpv6) {
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    } else {
        Ipv6GlobalRoutingHelper::PopulateRoutingTables();
    }

    if (tracing) {
        csma.EnablePcapAll("udp-echo", false);
        AsciiTraceHelper ascii;
        csma.EnableAsciiAll(ascii.CreateFileStream("udp-echo.tr"));
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}