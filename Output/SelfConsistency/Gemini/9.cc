#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingNetwork");

int main(int argc, char* argv[]) {
    LogComponent::SetLogLevel(LOG_PREFIX_TIME | LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point channels
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create net devices and channels for the ring topology
    NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices2 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices3 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer devices4 = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

    // Install the internet stack on the nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);
    Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);
    Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create UDP echo server on node 0
    UdpEchoServerHelper echoServer0(9);
    ApplicationContainer serverApps0 = echoServer0.Install(nodes.Get(0));
    serverApps0.Start(Seconds(0.0));
    serverApps0.Stop(Seconds(10.0));

    // Create UDP echo client on node 1
    UdpEchoClientHelper echoClient1(interfaces1.GetAddress(1), 9);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(1));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(10.0));

    // Create UDP echo server on node 2
    UdpEchoServerHelper echoServer2(9);
    ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(2));
    serverApps2.Start(Seconds(2.0));
    serverApps2.Stop(Seconds(10.0));

    // Create UDP echo client on node 3
    UdpEchoClientHelper echoClient3(interfaces3.GetAddress(0), 9);
    echoClient3.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps3 = echoClient3.Install(nodes.Get(3));
    clientApps3.Start(Seconds(3.0));
    clientApps3.Stop(Seconds(10.0));


    // Enable Tracing for PCAP files
    pointToPoint.EnablePcapAll("ring");

    // Animation Interface
    AnimationInterface anim("ring_network.xml");
    anim.SetConstantPosition(nodes.Get(0), 10, 10);
    anim.SetConstantPosition(nodes.Get(1), 30, 10);
    anim.SetConstantPosition(nodes.Get(2), 50, 10);
    anim.SetConstantPosition(nodes.Get(3), 70, 10);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}