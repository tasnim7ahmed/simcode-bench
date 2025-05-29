#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-ospf-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"

using namespace ns3;

int main (int argc, char *argv[])
{
    LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create (4); // n0, n1, n2, n3 in a square

    // Define point-to-point helpers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    // Connect nodes in a square topology
    // Square: n0--n1
    //         |    |
    //        n3--n2

    // Create link containers
    NetDeviceContainer ndc01 = p2p.Install (nodes.Get(0), nodes.Get(1)); // n0--n1
    NetDeviceContainer ndc12 = p2p.Install (nodes.Get(1), nodes.Get(2)); // n1--n2
    NetDeviceContainer ndc23 = p2p.Install (nodes.Get(2), nodes.Get(3)); // n2--n3
    NetDeviceContainer ndc30 = p2p.Install (nodes.Get(3), nodes.Get(0)); // n3--n0

    // Assign IP addresses
    InternetStackHelper stack;
    Ipv4OspfRoutingHelper ospf;
    stack.SetRoutingHelper (ospf);
    stack.Install (nodes);

    Ipv4AddressHelper address;

    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if01 = address.Assign (ndc01);

    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if12 = address.Assign (ndc12);

    address.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if23 = address.Assign (ndc23);

    address.SetBase ("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if30 = address.Assign (ndc30);

    // Enable pcap tracing per link
    p2p.EnablePcapAll ("square-ospf");

    uint16_t port = 4000;

    // Create a UDP server on node 2
    UdpServerHelper server (port);
    ApplicationContainer serverApp = server.Install (nodes.Get(2));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (10.0));

    // Create UDP client on node 0, directed to node 2
    Ipv4Address dstAddr = if12.GetAddress(1); // n2's interface towards n1
    UdpClientHelper client (dstAddr, port);
    client.SetAttribute ("MaxPackets", UintegerValue (10));
    client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    client.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApp = client.Install (nodes.Get(0));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (10.0));

    // Enable ASCII tracing for detailed packet info
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll (ascii.CreateFileStream ("square-ospf.tr"));

    Simulator::Stop (Seconds (11.0));
    Simulator::Run ();

    // Print routing tables
    Ipv4GlobalRoutingHelper g;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        std::cout << "Routing table for node " << i << " (" << ipv4->GetAddress(1,0).GetLocal () << "):" << std::endl;
        ipv4->GetRoutingProtocol()->PrintRoutingTable (Create<OutputStreamWrapper>(&std::cout));
        std::cout << std::endl;
    }

    Simulator::Destroy ();
    return 0;
}