#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PointToPointFourNodeExample");

int main (int argc, char *argv[])
{
    // Command line arguments
    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create (4); // n0, n1, n2, n3

    // Create point-to-point links
    // n0 <--> n2
    NodeContainer n0n2 = NodeContainer (nodes.Get (0), nodes.Get (2));
    // n1 <--> n2
    NodeContainer n1n2 = NodeContainer (nodes.Get (1), nodes.Get (2));
    // n2 <--> n3
    NodeContainer n2n3 = NodeContainer (nodes.Get (2), nodes.Get (3));

    PointToPointHelper p2p_5mbps;
    p2p_5mbps.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p_5mbps.SetChannelAttribute ("Delay", StringValue ("2ms"));
    p2p_5mbps.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("100p"));

    PointToPointHelper p2p_1_5mbps;
    p2p_1_5mbps.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
    p2p_1_5mbps.SetChannelAttribute ("Delay", StringValue ("10ms"));
    p2p_1_5mbps.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("100p"));

    // Install net devices
    NetDeviceContainer d_n0n2 = p2p_5mbps.Install (n0n2);
    NetDeviceContainer d_n1n2 = p2p_5mbps.Install (n1n2);
    NetDeviceContainer d_n2n3 = p2p_1_5mbps.Install (n2n3);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    // n0n2
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n0n2 = address.Assign (d_n0n2);

    // n1n2
    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n1n2 = address.Assign (d_n1n2);

    // n2n3
    address.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n2n3 = address.Assign (d_n2n3);
    
    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // Set up UDP CBR traffic from n0 to n3
    uint16_t cbrPort1 = 8000;
    UdpServerHelper cbrServer1 (cbrPort1);
    ApplicationContainer serverApps1 = cbrServer1.Install (nodes.Get (3));
    serverApps1.Start (Seconds (0.0));
    serverApps1.Stop (Seconds (10.0));

    UdpClientHelper cbrClient1 (i_n2n3.GetAddress (1), cbrPort1);
    cbrClient1.SetAttribute ("MaxPackets", UintegerValue (3200));
    cbrClient1.SetAttribute ("Interval", TimeValue (Seconds (210.0*8.0/448000.0))); // Derived from data rate/packet size
    cbrClient1.SetAttribute ("PacketSize", UintegerValue (210));
    ApplicationContainer clientApps1 = cbrClient1.Install (nodes.Get (0));
    clientApps1.Start (Seconds (0.1));
    clientApps1.Stop (Seconds (10.0));

    // Set up UDP CBR traffic from n3 to n1
    uint16_t cbrPort2 = 8001;
    UdpServerHelper cbrServer2 (cbrPort2);
    ApplicationContainer serverApps2 = cbrServer2.Install (nodes.Get (1));
    serverApps2.Start (Seconds (0.0));
    serverApps2.Stop (Seconds (10.0));

    UdpClientHelper cbrClient2 (i_n1n2.GetAddress (0), cbrPort2);
    cbrClient2.SetAttribute ("MaxPackets", UintegerValue (3200));
    cbrClient2.SetAttribute ("Interval", TimeValue (Seconds (210.0*8.0/448000.0))); // Derived from data rate/packet size
    cbrClient2.SetAttribute ("PacketSize", UintegerValue (210));
    ApplicationContainer clientApps2 = cbrClient2.Install (nodes.Get (3));
    clientApps2.Start (Seconds (0.1));
    clientApps2.Stop (Seconds (10.0));

    // Set up FTP/TCP flow from n0 to n3 (BulkSend application)
    uint16_t ftpPort = 21;
    Address ftpAddress (InetSocketAddress (i_n2n3.GetAddress (1), ftpPort));
    BulkSendHelper ftpSender ("ns3::TcpSocketFactory", ftpAddress);
    ftpSender.SetAttribute ("MaxBytes", UintegerValue (0));
    ApplicationContainer ftpClientApp = ftpSender.Install (nodes.Get (0));
    ftpClientApp.Start (Seconds (1.2));
    ftpClientApp.Stop (Seconds (1.35));
    
    // Corresponding sink at n3
    PacketSinkHelper ftpSink ("ns3::TcpSocketFactory",
                              InetSocketAddress (Ipv4Address::GetAny(), ftpPort));
    ApplicationContainer ftpSinkApp = ftpSink.Install (nodes.Get (3));
    ftpSinkApp.Start (Seconds (0.0));
    ftpSinkApp.Stop (Seconds (10.0));

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    p2p_5mbps.EnableAsciiAll (ascii.CreateFileStream ("simple-global-routing.tr"));
    p2p_1_5mbps.EnableAsciiAll (ascii.CreateFileStream ("simple-global-routing.tr"));

    // Run simulation
    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}