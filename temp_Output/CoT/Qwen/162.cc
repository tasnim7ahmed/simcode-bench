#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyTcpSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes for the ring topology
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links between each pair of adjacent nodes in a ring
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices[4];
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t next = (i + 1) % 4;
        devices[i] = p2p.Install(nodes.Get(i), nodes.Get(next));
    }

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to each node's interfaces
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];
    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Set up routing so that all nodes know how to reach each other
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up TCP server and client applications
    uint16_t port = 50000;

    // Server node: node index 1
    Ptr<Node> serverNode = nodes.Get(1);
    Address serverAddress(InetSocketAddress(interfaces[1].GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = packetSinkHelper.Install(serverNode);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Client node: node index 3
    OnOffHelper onoff("ns3::TcpSocketFactory", serverAddress);
    onoff.SetConstantRate(DataRate("1Mbps"), 512);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApp = onoff.Install(nodes.Get(3));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(9.0));

    // Setup NetAnim for visualization
    AnimationInterface anim("ring_topology_tcp.xml");
    anim.SetStartTime(Seconds(0));
    anim.SetStopTime(Seconds(10));

    // Enable packet metadata to see different types in animation
    anim.EnablePacketMetadata(true);

    // Manually assign positions to nodes for better visualization in a square ring layout
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // Node 0
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));  // Node 1
    positionAlloc->Add(Vector(10.0, 10.0, 0.0)); // Node 2
    positionAlloc->Add(Vector(0.0, 10.0, 0.0));  // Node 3

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}