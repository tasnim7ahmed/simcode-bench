#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedRoutingSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(7);

    NodeContainer p2pNodes1, p2pNodes2, csmaNodes;
    p2pNodes1.Add(nodes.Get(0));
    p2pNodes1.Add(nodes.Get(1));
    p2pNodes1.Add(nodes.Get(2));
    p2pNodes1.Add(nodes.Get(5));

    p2pNodes2.Add(nodes.Get(0));
    p2pNodes2.Add(nodes.Get(3));
    p2pNodes2.Add(nodes.Get(4));
    p2pNodes2.Add(nodes.Get(5));

    csmaNodes.Add(nodes.Get(2));
    csmaNodes.Add(nodes.Get(3));
    csmaNodes.Add(nodes.Get(4));
    csmaNodes.Add(nodes.Get(5));
    csmaNodes.Add(nodes.Get(6));

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer p2pDevices1, p2pDevices2, csmaDevices;

    for (uint32_t i = 0; i < p2pNodes1.GetN() - 1; ++i) {
        p2pDevices1.Add(pointToPoint.Install(NodeContainer(p2pNodes1.Get(i), p2pNodes1.Get(i + 1))));
    }

    for (uint32_t i = 0; i < p2pNodes2.GetN() - 1; ++i) {
        p2pDevices2.Add(pointToPoint.Install(NodeContainer(p2pNodes2.Get(i), p2pNodes2.Get(i + 1))));
    }

    csmaDevices = csma.Install(csmaNodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces1 = address.Assign(p2pDevices1);
    address.NewNetwork();
    Ipv4InterfaceContainer p2pInterfaces2 = address.Assign(p2pDevices2);
    address.NewNetwork();
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(csmaInterfaces.GetAddress(3), port));
    onoff.SetConstantRate(DataRate("1Mbps"), 512);
    ApplicationContainer app1 = onoff.Install(nodes.Get(1));
    app1.Start(Seconds(1.0));
    app1.Stop(Seconds(20.0));

    OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(csmaInterfaces.GetAddress(3), port + 1));
    onoff2.SetConstantRate(DataRate("1Mbps"), 512);
    ApplicationContainer app2 = onoff2.Install(nodes.Get(1));
    app2.Start(Seconds(11.0));
    app2.Stop(Seconds(20.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("packet-reception.tr");
    csmaDevices.Get(0)->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback(&PcapFileWrapper::Write, CreateObject<PcapFileWrapper>("queue-drops.pcap", PcapFile::DLT_EN10MB, 65535)));
    pointToPoint.EnablePcapAll("p2p-traffic");

    Simulator::Schedule(Seconds(5.0), &Ipv4InterfaceContainer::SetDown, &p2pInterfaces1, 1);
    Simulator::Schedule(Seconds(6.0), &Ipv4GlobalRoutingHelper::RecomputeRoutingTables, Ipv4GlobalRoutingHelper());
    Simulator::Schedule(Seconds(10.0), &Ipv4InterfaceContainer::SetUp, &p2pInterfaces1, 1);
    Simulator::Schedule(Seconds(10.0), &Ipv4GlobalRoutingHelper::RecomputeRoutingTables, Ipv4GlobalRoutingHelper());

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}