#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterTopology");

int main(int argc, char *argv[]) {
    // Log components
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes for routers A, B, and C
    NodeContainer routers;
    routers.Create(3);

    // Create point-to-point links: A <-> B and B <-> C
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Links: A-B and B-C
    NetDeviceContainer linkAB = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer linkBC = p2p.Install(routers.Get(1), routers.Get(2));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper address;

    // Subnets for the two point-to-point links
    address.SetBase("10.1.1.0", "255.255.255.252"); // /30 for A-B
    Ipv4InterfaceContainer interfacesAB = address.Assign(linkAB);

    address.SetBase("10.1.2.0", "255.255.255.252"); // /30 for B-C
    Ipv4InterfaceContainer interfacesBC = address.Assign(linkBC);

    // Manually assign /32 IP addresses to loopback-style interfaces on A and C
    Ipv4Address addrA("192.168.1.1");
    Ipv4Address addrC("192.168.2.1");

    // Create loopback devices on A and C
    Ptr<LoopbackNetDevice> deviceA = CreateObject<LoopbackNetDevice>();
    routers.Get(0)->AddDevice(deviceA);
    deviceA->SetAddress(Mac48Address::Allocate());
    Ptr<Ipv4> ipv4A = routers.Get(0)->GetObject<Ipv4>();
    int32_t ifIndexA = ipv4A->AddInterface(deviceA);
    ipv4A->AddAddress(ifIndexA, Ipv4InterfaceAddress(addrA, Ipv4Mask("/32")));
    ipv4A->SetMetric(ifIndexA, 1);
    ipv4A->SetUp(ifIndexA);

    Ptr<LoopbackNetDevice> deviceC = CreateObject<LoopbackNetDevice>();
    routers.Get(2)->AddDevice(deviceC);
    deviceC->SetAddress(Mac48Address::Allocate());
    Ptr<Ipv4> ipv4C = routers.Get(2)->GetObject<Ipv4>();
    int32_t ifIndexC = ipv4C->AddInterface(deviceC);
    ipv4C->AddAddress(ifIndexC, Ipv4InterfaceAddress(addrC, Ipv4Mask("/32")));
    ipv4C->SetMetric(ifIndexC, 1);
    ipv4C->SetUp(ifIndexC);

    // Populate routing tables using global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up UDP OnOff application from router A to router C
    uint16_t port = 9; // Discard port (Echo)

    // Packet sink on router C
    PacketSinkHelper packetSink("ns3::UdpSocketFactory",
                                InetSocketAddress(addrC, port));
    ApplicationContainer sinkApp = packetSink.Install(routers.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // OnOff UDP application on router A
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      InetSocketAddress(addrC, port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer app = onoff.Install(routers.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    // Enable packet tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("three-router.tr");
    p2p.EnableAsciiAll(stream);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}