#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SingleNodeUdpEcho");

int main (int argc, char *argv[])
{
    uint32_t packetSize = 1024;
    double interval = 1.0;
    uint32_t nPackets = 5;

    CommandLine cmd;
    cmd.AddValue ("packetSize", "Number of bytes per packet", packetSize);
    cmd.AddValue ("interval", "Interval between packets (seconds)", interval);
    cmd.AddValue ("nPackets", "Number of packets", nPackets);
    cmd.Parse (argc, argv);

    LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Nodes
    NodeContainer nodes;
    nodes.Create (1);

    // Create a loopback point-to-point channel using the LoopbackNetDevice
    // Alternatively, create a point-to-point between node 0 and itself
    // We'll create two devices on node 0, connected by a channel

    Ptr<Node> node = nodes.Get(0);

    NetDeviceContainer devices;
    Ptr<PointToPointNetDevice> dev1 = CreateObject<PointToPointNetDevice> ();
    Ptr<PointToPointNetDevice> dev2 = CreateObject<PointToPointNetDevice> ();
    Ptr<PointToPointChannel> channel = CreateObject<PointToPointChannel> ();

    // Set device/channel parameters
    dev1->SetAddress (Mac48Address::Allocate ());
    dev1->SetDataRate (DataRate ("5Mbps"));
    dev1->SetMtu (1500);
    dev1->SetInterframeGap (Seconds (0));
    dev2->SetAddress (Mac48Address::Allocate ());
    dev2->SetDataRate (DataRate ("5Mbps"));
    dev2->SetMtu (1500);
    dev2->SetInterframeGap (Seconds (0));
    channel->SetAttribute ("Delay", TimeValue (MilliSeconds (2)));

    dev1->Attach (channel);
    dev2->Attach (channel);

    node->AddDevice (dev1);
    node->AddDevice (dev2);

    devices.Add (dev1);
    devices.Add (dev2);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // UDP Echo Server on node0, bound to "10.1.1.1"
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer (echoPort);
    ApplicationContainer serverApps = echoServer.Install (nodes.Get(0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    // UDP Echo Client on node0, targets "10.1.1.1"
    UdpEchoClientHelper echoClient (interfaces.GetAddress(1), echoPort);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (nPackets));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
    ApplicationContainer clientApps = echoClient.Install (nodes.Get(0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    // Tracing
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("udp-echo-single-node.tr");
    dev1->TraceConnectWithoutContext ("PhyTxEnd", MakeBoundCallback ([] (Ptr<const Packet> packet, Ptr<OutputStreamWrapper> stream) {
        *stream->GetStream () << Simulator::Now ().GetSeconds () << " PhyTxEnd: Packet UID " << packet->GetUid () << std::endl;
    }, stream));
    dev2->TraceConnectWithoutContext ("PhyRxEnd", MakeBoundCallback ([] (Ptr<const Packet> packet, Ptr<OutputStreamWrapper> stream) {
        *stream->GetStream () << Simulator::Now ().GetSeconds () << " PhyRxEnd: Packet UID " << packet->GetUid () << std::endl;
    }, stream));

    Simulator::Stop (Seconds (11.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}