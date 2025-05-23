#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvMobility");

int main (int argc, char *argv[])
{
    // 1. Simulation Parameters
    uint32_t numNodes = 4;
    double simTime = 60.0; // seconds
    uint32_t packetSize = 1024; // bytes
    std::string dataRate = "1Mbps";

    CommandLine cmd(__FILE__);
    cmd.AddValue ("numNodes", "Number of nodes in the simulation", numNodes);
    cmd.AddValue ("simTime", "Total duration of the simulation in seconds", simTime);
    cmd.Parse (argc, argv);

    // 2. Configure Wi-Fi standards and fragmentation thresholds
    Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));

    // 3. Create nodes
    NS_LOG_INFO ("Creating " << numNodes << " nodes.");
    NodeContainer nodes;
    nodes.Create (numNodes);

    // 4. Install AODV routing protocol
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper (aodv);
    stack.Install (nodes);

    // 5. Configure Wi-Fi physical and MAC layers
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ); // Example: 802.11n for better performance

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.SetChannel (YansWifiChannelHelper::Default ());

    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

    // 6. Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // 7. Configure Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue (Rectangle (-100, 100, -100, 100)),
                               "Speed", StringValue ("ns3::UniformRandomVariable[Min=1|Max=5]"), // 1 to 5 m/s
                               "Pause", StringValue ("ns3::UniformRandomVariable[Min=0|Max=2]")); // 0 to 2 seconds pause
    mobility.Install (nodes);

    // 8. Create UDP Applications
    // Pair 1: Node 0 <-> Node 1 (bidirectional traffic)
    // Pair 2: Node 2 <-> Node 3 (bidirectional traffic)

    // Packet sinks to receive UDP traffic
    PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9));
    ApplicationContainer sinkApps;
    sinkApps.Add (sinkHelper.Install (nodes.Get (0))); // Sink on Node 0
    sinkApps.Add (sinkHelper.Install (nodes.Get (1))); // Sink on Node 1
    sinkApps.Add (sinkHelper.Install (nodes.Get (2))); // Sink on Node 2
    sinkApps.Add (sinkHelper.Install (nodes.Get (3))); // Sink on Node 3
    sinkApps.Start (Seconds (0.0));
    sinkApps.Stop (Seconds (simTime));

    // OnOff applications to send UDP traffic
    OnOffHelper onoffHelper ("ns3::UdpSocketFactory", Address ());
    onoffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onoffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    onoffHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
    onoffHelper.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));

    ApplicationContainer clientApps;

    // Node 0 sends to Node 1
    onoffHelper.SetAttribute ("Remote", AddressValue (InetSocketAddress (interfaces.GetAddress (1, 0), 9)));
    clientApps.Add (onoffHelper.Install (nodes.Get (0)));

    // Node 1 sends to Node 0
    onoffHelper.SetAttribute ("Remote", AddressValue (InetSocketAddress (interfaces.GetAddress (0, 0), 9)));
    clientApps.Add (onoffHelper.Install (nodes.Get (1)));

    // Node 2 sends to Node 3
    onoffHelper.SetAttribute ("Remote", AddressValue (InetSocketAddress (interfaces.GetAddress (3, 0), 9)));
    clientApps.Add (onoffHelper.Install (nodes.Get (2)));

    // Node 3 sends to Node 2
    onoffHelper.SetAttribute ("Remote", AddressValue (InetSocketAddress (interfaces.GetAddress (2, 0), 9)));
    clientApps.Add (onoffHelper.Install (nodes.Get (3)));

    clientApps.Start (Seconds (1.0)); // Start sending slightly after sinks are ready
    clientApps.Stop (Seconds (simTime - 1.0)); // Stop sending before simulation ends

    // 9. Enable NetAnim visualization
    NS_LOG_INFO ("Enabling NetAnim visualization.");
    AnimationInterface anim ("adhoc-aodv-mobility.xml");
    anim.EnablePacketMetadata (true);
    anim.EnableIpv4RouteTracking (true); // Track IPv4 routing table changes

    // 10. Enable FlowMonitor
    NS_LOG_INFO ("Enabling FlowMonitor.");
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    // 11. Run simulation
    NS_LOG_INFO ("Running simulation for " << simTime << " seconds.");
    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    // 12. Collect and analyze FlowMonitor statistics
    monitor->CheckForEnd (); // Ensure all latest stats are gathered
    monitor->SerializeToXmlFile("adhoc-aodv-mobility-flowmon.xml", true, true); // Save to XML for external analysis if needed

    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalLostPackets = 0;
    double totalRxBytes = 0;
    double totalDelaySumSeconds = 0;
    
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        
        totalTxPackets += i->second.txPackets;
        totalRxPackets += i->second.rxPackets;
        totalLostPackets += i->second.lostPackets;
        totalRxBytes += i->second.rxBytes;

        if (i->second.rxPackets > 0)
        {
            double delaySum = i->second.delaySum.GetSeconds();
            double avgDelay = delaySum / i->second.rxPackets;
            totalDelaySumSeconds += delaySum;

            double flowDuration = i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds();
            // Ensure flowDuration is positive to avoid division by zero or negative throughput
            if (flowDuration <= 0) {
                flowDuration = 0.000001; // A very small positive value
            }
            double throughput = i->second.rxBytes * 8.0 / flowDuration / 1024 / 1024;
            double pdr = (double)i->second.rxPackets / i->second.txPackets * 100.0;

            std::cout << "  Delay Sum: " << delaySum << " s\n";
            std::cout << "  Average Delay: " << avgDelay << " s\n";
            std::cout << "  Throughput: " << throughput << " Mbps\n";
            std::cout << "  Packet Delivery Ratio: " << pdr << " %\n";
        }
        else
        {
            std::cout << "  No packets received for this flow.\n";
            std::cout << "  Throughput: 0 Mbps\n";
            std::cout << "  Packet Delivery Ratio: 0 %\n";
        }
        std::cout << "\n";
    }

    // Overall metrics
    double overallPDR = (totalTxPackets > 0) ? (totalRxPackets / totalTxPackets * 100.0) : 0.0;
    // Overall throughput is total received bytes / total simulation time
    // using totalRxBytes (all flows) / simTime (overall simulation duration)
    double overallThroughput = totalRxBytes * 8.0 / (simTime * 1024 * 1024);
    
    // Overall Average End-to-End Delay calculation: sum of all delays / sum of all received packets
    double overallAvgDelay = (totalRxPackets > 0) ? (totalDelaySumSeconds / totalRxPackets) : 0.0;
    
    std::cout << "\n--- Overall Metrics ---\n";
    std::cout << "Total Tx Packets: " << totalTxPackets << "\n";
    std::cout << "Total Rx Packets: " << totalRxPackets << "\n";
    std::cout << "Total Lost Packets: " << totalLostPackets << "\n";
    std::cout << "Overall Packet Delivery Ratio: " << overallPDR << " %\n";
    std::cout << "Overall Throughput: " << overallThroughput << " Mbps\n";
    std::cout << "Overall Average End-to-End Delay: " << overallAvgDelay << " s\n";
    std::cout << "-----------------------\n";

    // 13. Cleanup
    Simulator::Destroy ();
    NS_LOG_INFO ("Done.");

    return 0;
}