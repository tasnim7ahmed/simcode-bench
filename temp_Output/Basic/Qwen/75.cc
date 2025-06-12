#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedTopology");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer p2pNodes0, p2pNodes1, csmaNodes, p2pNodes2;
    p2pNodes0.Add(NodeContainer::Create(2)); // n0 and n1 connected to n2 via point-to-point
    csmaNodes.Create(4);                    // n2, n3, n4, n5 on CSMA bus
    p2pNodes2.Add(NodeContainer::Create(2)); // n5 and n6 via point-to-point

    // Reorganize nodes for correct connections:
    // n0 -> n2 (p2p), n1 -> n2 (p2p), n2-n3-n4-n5 on CSMA, n5 -> n6 (p2p)
    NodeContainer n0n2 = NodeContainer(p2pNodes0.Get(0), csmaNodes.Get(0)); // n0 <-> n2
    NodeContainer n1n2 = NodeContainer(p2pNodes0.Get(1), csmaNodes.Get(0)); // n1 <-> n2
    NodeContainer n5n6 = NodeContainer(csmaNodes.Get(3), p2pNodes2.Get(0)); // n5 <-> n6

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer p2pDevices0 = pointToPoint.Install(n0n2);
    NetDeviceContainer p2pDevices1 = pointToPoint.Install(n1n2);
    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);
    NetDeviceContainer p2pDevices2 = pointToPoint.Install(n5n6);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces0 = address.Assign(p2pDevices0);
    address.NewNetwork();
    Ipv4InterfaceContainer p2pInterfaces1 = address.Assign(p2pDevices1);
    address.NewNetwork();
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);
    address.NewNetwork();
    Ipv4InterfaceContainer p2pInterfaces2 = address.Assign(p2pDevices2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint32_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(p2pInterfaces2.GetAddress(1), port));
    onoff.SetConstantRate(DataRate("448Kb/s"));
    onoff.SetAttribute("PacketSize", UintegerValue(210));

    ApplicationContainer apps = onoff.Install(p2pInterfaces0.Get(0).GetNode());
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sink.Install(p2pInterfaces2.Get(1).GetNode());
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper traceHelper;
    Ptr<OutputStreamWrapper> stream = traceHelper.CreateFileStream("mixed_topology.tr");
    pointToPoint.EnableAsciiAll(stream);
    csma.EnableAsciiAll(stream);

    pointToPoint.EnablePcapAll("mixed_topology");
    csma.EnablePcapAll("mixed_topology");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}