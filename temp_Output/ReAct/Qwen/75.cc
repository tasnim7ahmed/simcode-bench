#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedPointToPointCsmaSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer p2pNodes0, p2pNodes1, csmaNodes, p2pNodes2;
    p2pNodes0.Create(2); // n0 and n1 connected to n2 via P2P
    NodeContainer p2pHub = NodeContainer(p2pNodes0.Get(1));
    csmaNodes.Add(p2pHub); // n2 is part of CSMA bus
    csmaNodes.Create(3);   // n3, n4, n5
    p2pNodes2.Add(csmaNodes.Get(2)); // n5
    p2pNodes2.Create(1);   // n6

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices0, p2pDevices1, p2pDevices2;
    p2pDevices0 = pointToPoint.Install(p2pNodes0);
    p2pDevices1 = pointToPoint.Install(NodeContainer(p2pHub, p2pNodes1.Create(1)));
    p2pDevices2 = pointToPoint.Install(p2pNodes2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);

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

    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(csmaInterfaces.GetAddress(3), sinkPort)); // n5 -> n6

    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(p2pNodes2.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetConstantRate(DataRate("448Kbps"), 210);
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));

    ApplicationContainer apps = onoff.Install(p2pNodes0.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(9.0));

    AsciiTraceHelper asciiTraceHelper;
    pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("mixed-p2p-csma.tr"));
    csma.EnableAsciiAll(asciiTraceHelper.CreateFileStream("mixed-p2p-csma.tr"));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}