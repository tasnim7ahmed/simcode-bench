#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyUdpEcho");

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    // Assign constant positions for NetAnim
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(50.0, 100.0, 0.0));  // Node 0
    positionAlloc->Add(Vector(150.0, 100.0, 0.0)); // Node 1
    positionAlloc->Add(Vector(150.0, 0.0, 0.0));   // Node 2
    positionAlloc->Add(Vector(50.0, 0.0, 0.0));    // Node 3
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Create point-to-point links and store net devices and interfaces
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> devices(4);
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = p2p.Install(nodes.Get(2), nodes.Get(3));
    devices[3] = p2p.Install(nodes.Get(3), nodes.Get(0));

    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign net addresses for each link
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces(4);
    address.SetBase("192.9.39.0", "255.255.255.0");
    interfaces[0] = address.Assign(devices[0]);
    address.NewNetwork();
    interfaces[1] = address.Assign(devices[1]);
    address.NewNetwork();
    interfaces[2] = address.Assign(devices[2]);
    address.NewNetwork();
    interfaces[3] = address.Assign(devices[3]);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Application parameters
    uint16_t udpPort = 9;
    uint32_t packetSize = 1024;
    uint32_t maxPackets = 4;
    Time interPacketInterval = Seconds(0.5);

    // SERVER/CLIENT SCHEDULE
    // - Node 0 server, Node 1 client (start: 1s, stop: 3.5s)
    // - Node 1 server, Node 2 client (start: 4s, stop: 6.5s)
    // - Node 2 server, Node 3 client (start: 7s, stop: 9.5s)
    // - Node 3 server, Node 0 client (start: 10s, stop: 12.5s)

    // 1st pair: Node 0 as server, Node 1 as client
    UdpEchoServerHelper echoServer0(udpPort);
    ApplicationContainer serverApp0 = echoServer0.Install(nodes.Get(0));
    serverApp0.Start(Seconds(1.0));
    serverApp0.Stop(Seconds(3.5));

    Ipv4Address node0addr = nodes.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();
    UdpEchoClientHelper echoClient0(node0addr, udpPort);
    echoClient0.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient0.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient0.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp0 = echoClient0.Install(nodes.Get(1));
    clientApp0.Start(Seconds(1.5));
    clientApp0.Stop(Seconds(3.5));

    // 2nd pair: Node 1 as server, Node 2 as client
    UdpEchoServerHelper echoServer1(udpPort);
    ApplicationContainer serverApp1 = echoServer1.Install(nodes.Get(1));
    serverApp1.Start(Seconds(4.0));
    serverApp1.Stop(Seconds(6.5));

    Ipv4Address node1addr = nodes.Get(1)->GetObject<Ipv4>()->GetAddress(2,0).GetLocal();
    UdpEchoClientHelper echoClient1(node1addr, udpPort);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient1.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient1.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp1 = echoClient1.Install(nodes.Get(2));
    clientApp1.Start(Seconds(4.5));
    clientApp1.Stop(Seconds(6.5));

    // 3rd pair: Node 2 as server, Node 3 as client
    UdpEchoServerHelper echoServer2(udpPort);
    ApplicationContainer serverApp2 = echoServer2.Install(nodes.Get(2));
    serverApp2.Start(Seconds(7.0));
    serverApp2.Stop(Seconds(9.5));

    Ipv4Address node2addr = nodes.Get(2)->GetObject<Ipv4>()->GetAddress(2,0).GetLocal();
    UdpEchoClientHelper echoClient2(node2addr, udpPort);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient2.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient2.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp2 = echoClient2.Install(nodes.Get(3));
    clientApp2.Start(Seconds(7.5));
    clientApp2.Stop(Seconds(9.5));

    // 4th pair: Node 3 as server, Node 0 as client
    UdpEchoServerHelper echoServer3(udpPort);
    ApplicationContainer serverApp3 = echoServer3.Install(nodes.Get(3));
    serverApp3.Start(Seconds(10.0));
    serverApp3.Stop(Seconds(12.5));

    Ipv4Address node3addr = nodes.Get(3)->GetObject<Ipv4>()->GetAddress(2,0).GetLocal();
    UdpEchoClientHelper echoClient3(node3addr, udpPort);
    echoClient3.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient3.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient3.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp3 = echoClient3.Install(nodes.Get(0));
    clientApp3.Start(Seconds(10.5));
    clientApp3.Stop(Seconds(12.5));

    // NetAnim trace
    AnimationInterface anim("ring-topology.xml");

    Simulator::Stop(Seconds(14.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}