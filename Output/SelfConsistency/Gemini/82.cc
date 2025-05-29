#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoLan");

int main(int argc, char* argv[]) {
    bool useIpv6 = false;
    CommandLine cmd;
    cmd.AddValue("useIpv6", "Set to true to use IPv6", useIpv6);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(500)));
    csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv6AddressHelper address6;
    if (useIpv6) {
        address6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    } else {
        address.SetBase("10.1.1.0", "255.255.255.0");
    }

    InterfaceContainer interfaces;
    if (useIpv6) {
        interfaces = address6.Assign(devices);
        address6.NewNetwork();
    } else {
        interfaces = address.Assign(devices);
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    }

    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Trace queue length
    Ptr<Queue> queue = StaticCast<DropTailQueue>(devices.Get(0)->GetQueue());
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("udp-echo.tr"));

    // Enable pcap tracing
    csma.EnablePcapAll("udp-echo", false);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}