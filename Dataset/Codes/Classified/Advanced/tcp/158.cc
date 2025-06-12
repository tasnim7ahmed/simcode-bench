#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpExample");

int main (int argc, char *argv[])
{
    // Set up default simulation parameters
    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create (2);

    // Set up a point-to-point link between the nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install (nodes);

    // Install the Internet stack (TCP/IP) on both nodes
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Set up a TCP server on node 1
    uint16_t port = 8080; // Port number
    Address serverAddress (InetSocketAddress (interfaces.GetAddress (1), port));
    PacketSinkHelper tcpServerHelper ("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = tcpServerHelper.Install (nodes.Get (1));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (10.0));

    // Set up a TCP client on node 0 to send data to node 1
    OnOffHelper tcpClientHelper ("ns3::TcpSocketFactory", serverAddress);
    tcpClientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    tcpClientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    tcpClientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
    tcpClientHelper.SetAttribute ("PacketSize", UintegerValue (1024)); // Packet size: 1024 bytes

    ApplicationContainer clientApp = tcpClientHelper.Install (nodes.Get (0));
    clientApp.Start (Seconds (2.0)); // Start client after 2 seconds
    clientApp.Stop (Seconds (10.0)); // Stop after 10 seconds

    // Enable flow monitor to track performance statistics
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

    // Run the simulation
    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();

    // Print flow statistics (e.g., throughput, delay)
    monitor->CheckForLostPackets ();
    monitor->SerializeToXmlFile ("tcp-flow-monitor.xml", true, true);

    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        NS_LOG_UNCOND ("Flow " << i->first << " (" << serverAddress << "):");
        NS_LOG_UNCOND ("  Tx Packets: " << i->second.txPackets);
        NS_LOG_UNCOND ("  Rx Packets: " << i->second.rxPackets);
        NS_LOG_UNCOND ("  Throughput: " << i->second.rxBytes * 8.0 / 9.0 / 1024 / 1024 << " Mbps");
    }

    // Cleanup and exit
    Simulator::Destroy ();
    return 0;
}

