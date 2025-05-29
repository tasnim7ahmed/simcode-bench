#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpRenoCubicBottleneck");

int main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);

    // Create nodes: left1 -- bottleneck -- right1
    NodeContainer n0n1;
    n0n1.Create (2);
    Ptr<Node> left1 = n0n1.Get (0);
    Ptr<Node> bottleneck = n0n1.Get (1);

    NodeContainer n2;
    n2.Create (1);
    Ptr<Node> right1 = n2.Get (0);

    // Create P2P links
    PointToPointHelper left;
    left.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    left.SetChannelAttribute ("Delay", StringValue ("2ms"));

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute ("DataRate", StringValue ("2Mbps"));
    bottleneckLink.SetChannelAttribute ("Delay", StringValue ("20ms"));

    // Connect left1 <--10Mbps--> bottleneck <--2Mbps--> right1
    NetDeviceContainer devLeft = left.Install (left1, bottleneck);
    NetDeviceContainer devBottleneck = bottleneckLink.Install (bottleneck, right1);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install (left1);
    internet.Install (bottleneck);
    internet.Install (right1);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceLeft = address.Assign (devLeft);

    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceBottleneck = address.Assign (devBottleneck);

    // For the second TCP sender, we need an extra source node on left
    Ptr<Node> left2 = CreateObject<Node> ();
    internet.Install (left2);

    NetDeviceContainer devLeft2 = left.Install (left2, bottleneck);
    address.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceLeft2 = address.Assign (devLeft2);

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // Set up TCP variants
    // Flow 1: TCP Reno
    TypeId tcpRenoTypeId = TypeId::LookupByName ("ns3::TcpReno");
    // Flow 2: TCP Cubic
    TypeId tcpCubicTypeId = TypeId::LookupByName ("ns3::TcpCubic");

    // Sinks (use the same port for both, different IPs)
    uint16_t port1 = 50000;
    uint16_t port2 = 50001;

    // Install PacketSink on right1 for both ports
    Address sinkAddress1 (InetSocketAddress (ifaceBottleneck.GetAddress (1), port1));
    Address sinkAddress2 (InetSocketAddress (ifaceBottleneck.GetAddress (1), port2));

    PacketSinkHelper sinkHelper1 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port1));
    ApplicationContainer sinkApp1 = sinkHelper1.Install (right1);
    sinkApp1.Start (Seconds (0.0));
    sinkApp1.Stop (Seconds (10.0));

    PacketSinkHelper sinkHelper2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port2));
    ApplicationContainer sinkApp2 = sinkHelper2.Install (right1);
    sinkApp2.Start (Seconds (0.0));
    sinkApp2.Stop (Seconds (10.0));

    // Flow 1: left1 (TCP Reno) sends to right1:50000
    Ptr<Socket> srcSocket1 = Socket::CreateSocket (left1, TcpSocketFactory::GetTypeId ());
    srcSocket1->SetAttribute ("TcpSocketType", TypeIdValue (tcpRenoTypeId));
    OnOffHelper onoff1 ("ns3::TcpSocketFactory", sinkAddress1);
    onoff1.SetAttribute ("DataRate", DataRateValue (DataRate ("8Mbps")));
    onoff1.SetAttribute ("PacketSize", UintegerValue (1000));
    onoff1.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
    onoff1.SetAttribute ("StopTime", TimeValue (Seconds (9.0)));

    ApplicationContainer app1 = onoff1.Install (left1);

    // Flow 2: left2 (TCP Cubic) sends to right1:50001
    Ptr<Socket> srcSocket2 = Socket::CreateSocket (left2, TcpSocketFactory::GetTypeId ());
    srcSocket2->SetAttribute ("TcpSocketType", TypeIdValue (tcpCubicTypeId));
    OnOffHelper onoff2 ("ns3::TcpSocketFactory", sinkAddress2);
    onoff2.SetAttribute ("DataRate", DataRateValue (DataRate ("8Mbps")));
    onoff2.SetAttribute ("PacketSize", UintegerValue (1000));
    onoff2.SetAttribute ("StartTime", TimeValue (Seconds (1.5)));
    onoff2.SetAttribute ("StopTime", TimeValue (Seconds (9.5)));

    ApplicationContainer app2 = onoff2.Install (left2);

    // NetAnim visualization
    AnimationInterface anim ("tcp-bottleneck.xml");
    anim.SetConstantPosition (left1, 10, 30);
    anim.SetConstantPosition (left2, 10, 70);
    anim.SetConstantPosition (bottleneck, 50, 50);
    anim.SetConstantPosition (right1, 90, 50);

    // Tag bottleneck link
    anim.UpdateNodeDescription (bottleneck, "Bottleneck");
    anim.UpdateNodeColor (bottleneck, 255, 0, 0); // Red for bottleneck

    anim.UpdateNodeDescription (left1, "TCP Reno");
    anim.UpdateNodeDescription (left2, "TCP Cubic");
    anim.UpdateNodeDescription (right1, "Sink");

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}