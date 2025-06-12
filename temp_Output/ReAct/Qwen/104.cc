#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterNetworkSimulation");

int main(int argc, char *argv[]) {
    // Simulation parameters
    std::string dataRate = "1Mbps";
    std::string delay = "10ms";
    uint32_t packetSize = 1024;
    Time onOffInterval = Seconds(1);
    Time simulationDuration = Seconds(10);

    // Enable logging
    LogComponentEnable("ThreeRouterNetworkSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    Ptr<Node> nodeA = nodes.Get(0);
    Ptr<Node> nodeB = nodes.Get(1);
    Ptr<Node> nodeC = nodes.Get(2);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));

    // Links: A <-> B and B <-> C
    NetDeviceContainer devAB = p2p.Install(nodeA, nodeB);
    NetDeviceContainer devBC = p2p.Install(nodeB, nodeC);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses using /32 prefix
    Ipv4AddressHelper ipv4AB;
    ipv4AB.SetBase("192.168.1.0", "255.255.255.255"); // /32
    Ipv4InterfaceContainer ifAB = ipv4AB.Assign(devAB);

    Ipv4AddressHelper ipv4BC;
    ipv4BC.SetBase("192.168.2.0", "255.255.255.255"); // /32
    Ipv4InterfaceContainer ifBC = ipv4BC.Assign(devBC);

    // Set static routes
    Ipv4StaticRoutingHelper routingHelper;

    // Route from A to C via B
    Ptr<Ipv4StaticRouting> routeA = routingHelper.GetStaticRouting(nodeA->GetObject<Ipv4>());
    routeA->AddHostRouteTo(Ipv4Address("192.168.2.1"), Ipv4Address("192.168.1.2"), 1); // Assuming nodeC's interface is 192.168.2.1

    // Route from B to C (direct)
    Ptr<Ipv4StaticRouting> routeB = routingHelper.GetStaticRouting(nodeB->GetObject<Ipv4>());
    routeB->AddHostRouteTo(Ipv4Address("192.168.2.1"), Ipv4Address("192.168.2.2"), 2);

    // Create UDP application on node A to send packets to node C
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(ifBC.GetAddress(1), port)); // Node C's IP

    OnOffHelper onOffApp("ns3::UdpSocketFactory", sinkAddress);
    onOffApp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffApp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffApp.SetAttribute("DataRate", DataRateValue(DataRate("500kbps")));
    onOffApp.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer appContainer = onOffApp.Install(nodeA);
    appContainer.Start(Seconds(1.0));
    appContainer.Stop(simulationDuration);

    // Create PacketSink on node C
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodeC);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(simulationDuration);

    // Enable tracing
    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("three-router-network.tr"));
    p2p.EnablePcapAll("three-router-network");

    // Setup NetAnim for visualization
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(1.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    AnimationInterface anim("three-router-network.xml");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}