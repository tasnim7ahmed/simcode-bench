#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FragmentationIPv6Simulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices0r = csma.Install(NodeContainer(nodes.Get(0), nodes.Get(2)));
    NetDeviceContainer devices1r = csma.Install(NodeContainer(nodes.Get(1), nodes.Get(2)));

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;
    Ipv6InterfaceContainer interfaces0r;
    Ipv6InterfaceContainer interfaces1r;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    interfaces0r = ipv6.Assign(devices0r);
    interfaces0r.SetForwarding(2, true);
    interfaces0r.SetDefaultRouteInAllNodes(2);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    interfaces1r = ipv6.Assign(devices1r);
    interfaces1r.SetForwarding(2, true);
    interfaces1r.SetDefaultRouteInAllNodes(2);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces1r.GetAddress(0, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(2000));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("fragmentation-ipv6.tr");
    csma.EnableAsciiAll(stream);
    csma.EnablePcapAll("fragmentation-ipv6");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}