#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyTcpPersistent");

int main (int argc, char *argv[])
{
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create 4 nodes (ring topology)
    NodeContainer nodes;
    nodes.Create(4);

    // Point-to-point helper parameters
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // Set up the ring connections: 0-1, 1-2, 2-3, 3-0
    NetDeviceContainer d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer d30 = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign addresses to the point-to-point devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i01 = address.Assign(d01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i12 = address.Assign(d12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = address.Assign(d23);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i30 = address.Assign(d30);

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install a TCP server on node 2
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(i12.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.5));
    sinkApp.Stop(Seconds(20.0));

    // Install a persistent TCP (OnOff) client on node 0 towards node 2
    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("DataRate", StringValue("2Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=20.0]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(20.0));

    // Enable packet-level tracing on all devices
    p2p.EnablePcapAll("ring-topology-tcp");

    // NetAnim setup: set node positions for clear visualization
    AnimationInterface anim("ring-topology-tcp.xml");
    anim.SetConstantPosition(nodes.Get(0), 50.0, 50.0);
    anim.SetConstantPosition(nodes.Get(1), 150.0, 50.0);
    anim.SetConstantPosition(nodes.Get(2), 150.0, 150.0);
    anim.SetConstantPosition(nodes.Get(3), 50.0, 150.0);

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}