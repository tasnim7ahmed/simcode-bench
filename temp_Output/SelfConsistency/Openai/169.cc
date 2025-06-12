#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

static void
ThroughputMonitor (Ptr<Application> app, Ptr<PacketSink> sink, double lastTotalRx)
{
    double cur = Simulator::Now ().GetSeconds ();
    double curTotalRx = sink->GetTotalRx ();
    double throughput = ((curTotalRx - lastTotalRx) * 8.0) / 1e6; // Mbps

    std::cout << "Time: " << cur << "s, "
              << "Throughput: " << throughput << " Mbps" << std::endl;

    Simulator::Schedule (Seconds (1.0), &ThroughputMonitor, app, sink, curTotalRx);
}

int
main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create (2);

    // Set up point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install (nodes);

    // Install IPv4 stack
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Create a PacketSink application on node 1 (server)
    uint16_t port = 8080;
    Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (1));
    sinkApp.Start (Seconds (0.0));
    sinkApp.Stop (Seconds (10.0));

    // Create a BulkSendApplication on node 0 (client)
    BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory", sinkAddress);
    bulkSendHelper.SetAttribute ("MaxBytes", UintegerValue (0)); // Unlimited
    ApplicationContainer clientApp = bulkSendHelper.Install (nodes.Get (0));
    clientApp.Start (Seconds (1.0));
    clientApp.Stop (Seconds (10.0));

    // Enable pcap tracing
    pointToPoint.EnablePcapAll ("p2p-tcp");

    // Throughput monitor
    Simulator::Schedule (Seconds (1.1), &ThroughputMonitor,
                         clientApp.Get (0),
                         DynamicCast<PacketSink> (sinkApp.Get (0)),
                         0);

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}