#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoExample");

int main(int argc, char* argv[]) {
    bool useIpv4 = true;
    bool verbose = false;

    CommandLine cmd;
    cmd.AddValue("useIpv4", "Set to false to disable IPv4 and use IPv6", useIpv4);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.Parse(argc, argv);

    LogComponent::SetAttribute("UdpEchoClientApplication", "Verbose", BooleanValue(verbose));
    LogComponent::SetAttribute("UdpEchoServerApplication", "Verbose", BooleanValue(verbose));

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv6AddressHelper address6;
    Ipv4InterfaceContainer interfaces;
    Ipv6InterfaceContainer interfaces6;

    if (useIpv4) {
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    }

    address6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    interfaces6 = address6.Assign(devices);
    Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(11.0));

    UdpEchoClientHelper echoClient(useIpv4 ? interfaces.GetAddress(1) : interfaces6.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    csma.EnableQueueDisc("fifo", "ns3::FifoQueueDisc", devices);
    csma.EnablePcapAll("udp-echo", false);

    Simulator::Stop(Seconds(12.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("udp-echo.tr"));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}