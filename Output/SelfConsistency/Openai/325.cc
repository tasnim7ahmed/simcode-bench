#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpRttExample");

// RTT measurement callback function
void
RttTracer (Ptr<OutputStreamWrapper> stream, Time oldRtt, Time newRtt)
{
    *stream->GetStream() << Simulator::Now ().GetSeconds () << "s\tRTT: " << newRtt.GetMilliSeconds () << " ms" << std::endl;
}

int
main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);
    LogComponentEnable ("TcpRttExample", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create (2);

    // Set up the point-to-point channel
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install (nodes);

    // Install the internet stack
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Create a TCP server (PacketSink) on node 1
    uint16_t serverPort = 8080;
    Address serverAddress (InetSocketAddress (Ipv4Address::GetAny (), serverPort));
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (1));
    sinkApp.Start (Seconds (1.0));
    sinkApp.Stop (Seconds (10.0));

    // Create a TCP client (BulkSend) on node 0
    Address clientAddress (InetSocketAddress (interfaces.GetAddress (1), serverPort));
    BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory", clientAddress);
    bulkSendHelper.SetAttribute ("MaxBytes", UintegerValue (0)); // Unlimited
    ApplicationContainer clientApp = bulkSendHelper.Install (nodes.Get (0));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (10.0));

    // Enable RTT tracing
    Ptr<Socket> tcpSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
    // Set TCP socket attributes if needed

    // Open an output stream for RTT log
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("tcp-rtt-log.txt");

    // Connect socket to RttTracer callback
    Config::ConnectWithoutContext (
        "/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RTT",
        MakeBoundCallback (&RttTracer, stream)
    );

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}