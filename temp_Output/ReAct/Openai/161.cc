#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpUdpComparison");

int
main (int argc, char *argv[])
{
    // Simulation parameters
    double simTime = 10.0;
    std::string dataRate = "10Mbps";
    std::string delay = "5ms";
    uint32_t packetSize = 1024;
    uint32_t numPackets = 10000;
    uint16_t tcpPort = 50000;
    uint16_t udpPort = 40000;

    // Create nodes
    NodeContainer nodes;
    nodes.Create (2);

    // Install point-to-point devices and channel
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue (dataRate));
    p2p.SetChannelAttribute ("Delay", StringValue (delay));
    NetDeviceContainer devices = p2p.Install (nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // ----------------------
    // Install TCP Application
    // ----------------------

    // TCP Server (PacketSink) on node 1
    PacketSinkHelper tcpSinkHelper ("ns3::TcpSocketFactory",
                                    InetSocketAddress (Ipv4Address::GetAny (), tcpPort));
    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install (nodes.Get (1));
    tcpSinkApp.Start (Seconds (0.0));
    tcpSinkApp.Stop (Seconds (simTime));

    // TCP Client (BulkSendApplication) on node 0
    BulkSendHelper tcpSource ("ns3::TcpSocketFactory",
                              InetSocketAddress (interfaces.GetAddress (1), tcpPort));
    tcpSource.SetAttribute ("MaxBytes", UintegerValue (packetSize * numPackets));
    tcpSource.SetAttribute ("SendSize", UintegerValue (packetSize));
    ApplicationContainer tcpSourceApp = tcpSource.Install (nodes.Get (0));
    tcpSourceApp.Start (Seconds (1.0));
    tcpSourceApp.Stop (Seconds (simTime - 1));

    // ----------------------
    // Install UDP Application
    // ----------------------

    // UDP Server (PacketSink) on node 1
    PacketSinkHelper udpSinkHelper ("ns3::UdpSocketFactory",
                                    InetSocketAddress (Ipv4Address::GetAny (), udpPort));
    ApplicationContainer udpSinkApp = udpSinkHelper.Install (nodes.Get (1));
    udpSinkApp.Start (Seconds (0.0));
    udpSinkApp.Stop (Seconds (simTime));

    // UDP Client (UdpClientApplication) on node 0
    UdpClientHelper udpClient (interfaces.GetAddress (1), udpPort);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
    udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.001)));
    udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
    ApplicationContainer udpClientApp = udpClient.Install (nodes.Get (0));
    udpClientApp.Start (Seconds (1.0));
    udpClientApp.Stop (Seconds (simTime - 1));

    // Mobility for NetAnim visualization (no movement, just positions)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // Node 0
    positionAlloc->Add (Vector (20.0, 0.0, 0.0)); // Node 1
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Enable NetAnim tracing
    AnimationInterface anim ("tcp-udp-comparison.xml");

    // Enable FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll ();

    // Enable pcap tracing for visualization
    p2p.EnablePcapAll ("tcp-udp-comparison");

    // Run simulation
    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    // FlowMonitor statistics
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

    double tcpThroughput = 0.0;
    double tcpDelay = 0.0;
    uint32_t tcpRxPackets = 0, tcpTxPackets = 0;
    uint32_t tcpLostPackets = 0;
    uint32_t tcpFlows = 0;

    double udpThroughput = 0.0;
    double udpDelay = 0.0;
    uint32_t udpRxPackets = 0, udpTxPackets = 0;
    uint32_t udpLostPackets = 0;
    uint32_t udpFlows = 0;

    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);

        if (t.destinationPort == tcpPort)
        {
            tcpFlows++;
            tcpRxPackets += flow.second.rxPackets;
            tcpTxPackets += flow.second.txPackets;
            tcpLostPackets += flow.second.lostPackets;
            if (flow.second.rxPackets > 0)
            {
                tcpThroughput += (flow.second.rxBytes * 8.0) / (simTime - 2.0) / 1e6; // Mbps, ignoring 1s ramp up & 1s tail
                tcpDelay += (flow.second.delaySum.GetSeconds () / flow.second.rxPackets);
            }
        }
        else if (t.destinationPort == udpPort)
        {
            udpFlows++;
            udpRxPackets += flow.second.rxPackets;
            udpTxPackets += flow.second.txPackets;
            udpLostPackets += flow.second.lostPackets;
            if (flow.second.rxPackets > 0)
            {
                udpThroughput += (flow.second.rxBytes * 8.0) / (simTime - 2.0) / 1e6; // Mbps
                udpDelay += (flow.second.delaySum.GetSeconds () / flow.second.rxPackets);
            }
        }
    }

    if (tcpFlows > 0) tcpDelay /= tcpFlows;
    if (udpFlows > 0) udpDelay /= udpFlows;

    std::cout << "\n======== TCP vs UDP Performance Metrics ========" << std::endl;
    std::cout << "Metric            \tTCP\t\tUDP" << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;
    std::cout << "Throughput (Mbps) \t" << tcpThroughput << "\t\t" << udpThroughput << std::endl;
    std::cout << "Average Delay (s) \t" << tcpDelay << "\t\t" << udpDelay << std::endl;
    std::cout << "Delivered Packets \t" << tcpRxPackets << "\t\t" << udpRxPackets << std::endl;
    std::cout << "Sent Packets      \t" << tcpTxPackets << "\t\t" << udpTxPackets << std::endl;
    std::cout << "Lost Packets      \t" << tcpLostPackets << "\t\t" << udpLostPackets << std::endl;
    std::cout << "Packet Delivery Ratio \t";
    std::cout << (tcpTxPackets > 0 ? (double)tcpRxPackets / tcpTxPackets * 100 : 0) << "%\t\t";
    std::cout << (udpTxPackets > 0 ? (double)udpRxPackets / udpTxPackets * 100 : 0) << "%" << std::endl;
    std::cout << "===============================================\n" << std::endl;

    Simulator::Destroy ();
    return 0;
}