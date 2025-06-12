#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedP2pCsmaSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer p2pNodes01;
    p2pNodes01.Create(2);  // n0 and n1

    Ptr<Node> n2 = CreateObject<Node>();  // central hub node n2
    NodeContainer csmaNodes;
    csmaNodes.Add(n2);
    csmaNodes.Create(3);  // adds n3, n4, n5

    NodeContainer p2pNode6;
    p2pNode6.Create(1);  // n6

    // Point-to-point links between n0 -> n2 and n1 -> n2
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices02 = p2p.Install(p2pNodes01.Get(0), n2);
    NetDeviceContainer p2pDevices12 = p2p.Install(p2pNodes01.Get(1), n2);

    // CSMA/CD bus for n2, n3, n4, n5
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6560)));

    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    // Point-to-point link between n5 -> n6
    NetDeviceContainer p2pDevices56 = p2p.Install(csmaNodes.Get(3), p2pNode6.Get(0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces02 = address.Assign(p2pDevices02);

    address.NewNetwork();
    Ipv4InterfaceContainer p2pInterfaces12 = address.Assign(p2pDevices12);

    address.NewNetwork();
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    address.NewNetwork();
    Ipv4InterfaceContainer p2pInterfaces56 = address.Assign(p2pDevices56);

    // Set up CBR traffic from n0 to n6
    uint16_t port = 9;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(p2pNode6.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(p2pInterfaces56.GetAddress(1), port));
    onoff.SetConstantRate(DataRate("448Kbps"), 210);
    onoff.SetAttribute("PacketSize", UintegerValue(210));

    ApplicationContainer app = onoff.Install(p2pNodes01.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable tracing
    AsciiTraceHelper traceHelper;
    Ptr<OutputStreamWrapper> stream = traceHelper.CreateFileStream("mixed-p2p-csma.tr");
    p2p.EnableAsciiAll(stream);
    csma.EnableAsciiAll(stream);

    p2p.EnablePcapAll("mixed-p2p-csma");
    csma.EnablePcapAll("mixed-p2p-csma");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}