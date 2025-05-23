#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <string>
#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DumbbellRedAred");

int main(int argc, char* argv[])
{
    // 1. Set up configurable parameters and command-line arguments
    uint32_t numLeftNodes = 1;
    uint32_t numRightNodes = 1;
    std::string queueType = "RED"; // "RED" or "ARED"
    uint32_t queuePackets = 100;
    uint32_t packetSize = 1000; // bytes
    std::string dataRate = "1Mbps";
    double minTh = 5;
    double maxTh = 15;
    double dropP = 0.1;
    double aredK = 1.5;
    double aredAlpha = 0.05;
    double aredBeta = 0.9;
    std::string bottleneckBandwidth = "10Mbps";
    std::string bottleneckDelay = "2ms";
    std::string leafBandwidth = "100Mbps";
    std::string leafDelay = "1ms";
    double simulationTime = 10.0; // seconds
    bool tracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numLeftNodes", "Number of left-side leaf nodes", numLeftNodes);
    cmd.AddValue("numRightNodes", "Number of right-side leaf nodes", numRightNodes);
    cmd.AddValue("queueType", "Type of queue disc (RED or ARED)", queueType);
    cmd.AddValue("queuePackets", "Maximum packets allowed in the queue", queuePackets);
    cmd.AddValue("packetSize", "Application packet size in bytes", packetSize);
    cmd.AddValue("dataRate", "Application data rate", dataRate);
    cmd.AddValue("minTh", "RED/ARED minimum threshold", minTh);
    cmd.AddValue("maxTh", "RED/ARED maximum threshold", maxTh);
    cmd.AddValue("dropP", "RED/ARED drop probability (for RED)", dropP);
    cmd.AddValue("k", "ARED 'k' parameter", aredK);
    cmd.AddValue("alpha", "ARED 'alpha' parameter", aredAlpha);
    cmd.AddValue("beta", "ARED 'beta' parameter", aredBeta);
    cmd.AddValue("bottleneckBandwidth", "Bottleneck link bandwidth", bottleneckBandwidth);
    cmd.AddValue("bottleneckDelay", "Bottleneck link delay", bottleneckDelay);
    cmd.AddValue("leafBandwidth", "Leaf link bandwidth", leafBandwidth);
    cmd.AddValue("leafDelay", "Leaf link delay", leafDelay);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.AddValue("tracing", "Enable PCAP tracing", tracing);
    cmd.Parse(argc, argv);

    // Validate queueType
    if (queueType != "RED" && queueType != "ARED")
    {
        NS_FATAL_ERROR("Invalid queueType: " << queueType << ". Must be 'RED' or 'ARED'.");
    }

    // 2. Configure default settings for RED QueueDisc
    Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(minTh));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(maxTh));
    Config::SetDefault("ns3::RedQueueDisc::DropP", DoubleValue(dropP));
    Config::SetDefault("ns3::RedQueueDisc::MeanPktSize", UintegerValue(packetSize));
    Config::SetDefault("ns3::RedQueueDisc::QW", DoubleValue(0.002)); // Default QW for RED
    
    // Configure for ARED if specified
    if (queueType == "ARED")
    {
        Config::SetDefault("ns3::RedQueueDisc::AdaptiveRed", BooleanValue(true));
        Config::SetDefault("ns3::RedQueueDisc::K", DoubleValue(aredK));
        Config::SetDefault("ns3::RedQueueDisc::Alpha", DoubleValue(aredAlpha));
        Config::SetDefault("ns3::RedQueueDisc::Beta", DoubleValue(aredBeta));
    }
    else // RED
    {
        Config::SetDefault("ns3::RedQueueDisc::AdaptiveRed", BooleanValue(false));
    }

    // 3. Create nodes
    NodeContainer leftNodes;
    leftNodes.Create(numLeftNodes);
    NodeContainer rightNodes;
    rightNodes.Create(numRightNodes);
    NodeContainer coreNodes;
    coreNodes.Create(2); // c0 and c1

    // 4. Install Internet Stack
    InternetStackHelper internet;
    internet.Install(leftNodes);
    internet.Install(rightNodes);
    internet.Install(coreNodes);

    // 5. Create links and assign IP addresses
    PointToPointHelper p2pLeaf;
    p2pLeaf.SetDeviceAttribute("DataRate", StringValue(leafBandwidth));
    p2pLeaf.SetChannelAttribute("Delay", StringValue(leafDelay));

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(bottleneckBandwidth));
    p2pBottleneck.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    Ipv4AddressHelper ipv4;

    // Left-Core0 links
    std::vector<NetDeviceContainer> leftCoreDevices(numLeftNodes);
    std::vector<Ipv4InterfaceContainer> leftCoreInterfaces(numLeftNodes);
    for (uint32_t i = 0; i < numLeftNodes; ++i)
    {
        leftCoreDevices[i] = p2pLeaf.Install(leftNodes.Get(i), coreNodes.Get(0));
        std::string leftNet = "10.1." + std::to_string(i + 1) + ".0";
        ipv4.SetBase(leftNet.c_str(), "255.255.255.0");
        leftCoreInterfaces[i] = ipv4.Assign(leftCoreDevices[i]);
    }

    // Bottleneck link (Core0-Core1)
    NetDeviceContainer coreDevices = p2pBottleneck.Install(coreNodes.Get(0), coreNodes.Get(1));
    ipv4.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer coreInterfaces = ipv4.Assign(coreDevices);

    // Right-Core1 links
    std::vector<NetDeviceContainer> rightCoreDevices(numRightNodes);
    std::vector<Ipv4InterfaceContainer> rightCoreInterfaces(numRightNodes);
    for (uint32_t i = 0; i < numRightNodes; ++i)
    {
        rightCoreDevices[i] = p2pLeaf.Install(rightNodes.Get(i), coreNodes.Get(1));
        std::string rightNet = "10.3." + std::to_string(i + 1) + ".0";
        ipv4.SetBase(rightNet.c_str(), "255.255.255.0");
        rightCoreInterfaces[i] = ipv4.Assign(rightCoreDevices[i]);
    }

    // 6. Install Queue Disc on Bottleneck link (c0 side, outgoing towards c1)
    // The queue disc manages outgoing packets. So, it should be on c0's device towards c1.
    Ptr<PointToPointNetDevice> c0_c1_dev = DynamicCast<PointToPointNetDevice>(coreDevices.Get(0));
    NS_ASSERT_MSG(c0_c1_dev, "Failed to cast coreDevices.Get(0) to PointToPointNetDevice");

    TrafficControlHelper tc;
    // Set the overall QueueDisc's MaxPackets attribute, which affects PacketQueueLimit.
    tc.Set("ns3::QueueDisc", "MaxPackets", UintegerValue(queuePackets));
    tc.SetRootQueueDisc("ns3::RedQueueDisc");

    // Install on the first device of the bottleneck link (coreNodes.Get(0) side)
    QueueDiscContainer queueDiscs = tc.Install(c0_c1_dev);
    // Ensure only one queue disc was installed
    NS_ASSERT(queueDiscs.GetN() == 1);
    Ptr<QueueDisc> bottleneckQueueDisc = queueDiscs.Get(0);

    NS_LOG_INFO("Configured " << queueType << " QueueDisc on core0-core1 link (Node " << c0_c1_dev->GetNode()->GetId()
                               << ", Device " << c0_c1_dev->GetIfIndex() << ").");
    NS_LOG_INFO("  Queue Limit: " << queuePackets << " packets.");
    if (queueType == "RED")
    {
        NS_LOG_INFO("  RED MinTh: " << minTh << ", MaxTh: " << maxTh << ", DropP: " << dropP);
    }
    else // ARED
    {
        NS_LOG_INFO("  ARED MinTh: " << minTh << ", MaxTh: " << maxTh << ", K: " << aredK << ", Alpha: " << aredAlpha << ", Beta: " << aredBeta);
    }

    // 7. Install applications (TCP OnOff and PacketSink)
    uint16_t sinkPort = 9000;

    // Create a vector to store the IP addresses of the right-side nodes (PacketSink servers)
    std::vector<Ipv4Address> rightNodeAddresses;
    for (uint32_t i = 0; i < numRightNodes; ++i)
    {
        rightNodeAddresses.push_back(rightCoreInterfaces[i].GetAddress(0)); // Get the IP address of the device connected to core1
    }

    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    for (uint32_t i = 0; i < numLeftNodes; ++i)
    {
        // OnOff applications (clients on left nodes)
        OnOffHelper onoff("ns3::TcpSocketFactory", Address());
        onoff.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=1.5]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=1.5]"));
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
        onoff.SetAttribute("DataRate", StringValue(dataRate));

        // Each left node connects to a different right node in a round-robin fashion
        // if numLeftNodes > numRightNodes, multiple left nodes will connect to the same right node
        uint32_t targetRightNodeIdx = i % numRightNodes;
        Address sinkAddress(InetSocketAddress(rightNodeAddresses[targetRightNodeIdx], sinkPort));
        onoff.SetAttribute("Remote", AddressValue(sinkAddress));
        clientApps.Add(onoff.Install(leftNodes.Get(i)));
    }

    // PacketSink applications (servers on right nodes)
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    serverApps.Add(sink.Install(rightNodes)); // Installs a sink on each right node

    // Start/Stop applications
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime - 1.0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // 8. Tracing
    if (tracing)
    {
        p2pLeaf.EnablePcapAll("dumbbell-leaf", true);
        p2pBottleneck.EnablePcapAll("dumbbell-bottleneck", true);
    }

    // Enable FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // 9. Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // 10. Display final queue disc statistics and check for expected packet drops
    uint64_t totalDroppedPackets = 0; 
    std::cout << "\n--- Queue Disc Statistics ---\n";
    for (uint32_t i = 0; i < queueDiscs.GetN(); ++i)
    {
        Ptr<QueueDisc> qd = queueDiscs.Get(i);
        QueueDisc::Stats stats = qd->GetStats();
        std::cout << "QueueDisc on Node " << qd->GetNetDevice()->GetNode()->GetId()
                  << ", Device " << qd->GetNetDevice()->GetIfIndex()
                  << " (outgoing from " << qd->GetNetDevice()->GetNode()->GetName() << "):\n";
        std::cout << "  Packets Enqueued: " << stats.GetNPacketsEnqueued() << "\n";
        std::cout << "  Bytes Enqueued:   " << stats.GetNBytesEnqueued() << "\n";
        std::cout << "  Packets Dequeued: " << stats.GetNPacketsDequeued() << "\n";
        std::cout << "  Bytes Dequeued:   " << stats.GetNBytesDequeued() << "\n";
        std::cout << "  Packets Dropped:  " << stats.GetNPacketsDropped() << "\n";
        std::cout << "  Bytes Dropped:    " << stats.GetNBytesDropped() << "\n";
        std::cout << "  Current Queue Length: " << qd->GetCurrentSize().GetPackets() << " packets\n";
        totalDroppedPackets += stats.GetNPacketsDropped();
    }
    std::cout << "---------------------------\n";

    // Flow monitor statistics (optional, but useful for verification)
    monitor->CheckForErrors(); 
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    if (!classifier)
    {
        NS_LOG_WARN("FlowMonitor classifier is null. Cannot get flow statistics.");
    }
    else
    {
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStatsMap();
        std::cout << "\n--- Flow Monitor Statistics ---\n";
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->GetFiveTuple(i->first);
            std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
            std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
            std::cout << "  Lost Packets (end-to-end): " << i->second.lostPackets << "\n";
            if (i->second.timeLastRxPacket.GetSeconds() > i->second.timeFirstTxPacket.GetSeconds())
            {
                std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
            }
            else
            {
                std::cout << "  Throughput: 0 Mbps (No packets received or duration is zero)\n";
            }
        }
        std::cout << "---------------------------\n";
    }

    // Check for expected packet drops
    if (totalDroppedPackets > 0)
    {
        std::cout << "\n--- Congestion Detected ---\n";
        std::cout << "Total packets dropped by queue disc: " << totalDroppedPackets << "\n";
        std::cout << "This is expected when the network capacity is exceeded or RED/ARED starts dropping packets.\n";
    }
    else
    {
        std::cout << "\n--- No Queue Disc Drops ---\n";
        std::cout << "No packets reported dropped by the queue disc during the simulation.\n";
    }

    Simulator::Destroy();

    return 0;
}

} // namespace ns3