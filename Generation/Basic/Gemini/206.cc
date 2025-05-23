#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cmath> // For M_PI

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numSensorNodes = 5;
    double simulationTime = 60.0; // seconds
    double packetInterval = 5.0; // seconds (send one packet every 5 seconds)
    uint32_t packetSize = 100; // bytes
    double starRadius = 10.0; // meters

    CommandLine cmd(__FILE__);
    cmd.AddValue("numSensorNodes", "Number of sensor nodes (excluding sink)", numSensorNodes);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.AddValue("packetInterval", "Time interval between packets sent by sensors", packetInterval);
    cmd.AddValue("packetSize", "Size of data packets in bytes", packetSize);
    cmd.AddValue("starRadius", "Radius of the star topology in meters", starRadius);
    cmd.Parse(argc, argv);

    if (numSensorNodes == 0)
    {
        numSensorNodes = 1;
    }

    NodeContainer allNodes;
    allNodes.Create(numSensorNodes + 1); // +1 for the central sink node

    Ptr<Node> sinkNode = allNodes.Get(0);
    NodeContainer sensorNodes;
    for (uint32_t i = 0; i < numSensorNodes; ++i)
    {
        sensorNodes.Add(allNodes.Get(i + 1));
    }

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    Ptr<ConstantPositionMobilityModel> sinkMobility = sinkNode->GetObject<ConstantPositionMobilityModel>();
    sinkMobility->SetPosition(Vector(0.0, 0.0, 0.0));

    for (uint32_t i = 0; i < numSensorNodes; ++i)
    {
        Ptr<Node> sensorNode = sensorNodes.Get(i);
        Ptr<ConstantPositionMobilityModel> sensorMobility = sensorNode->GetObject<ConstantPositionMobilityModel>();

        double angle = (2.0 * M_PI * i) / numSensorNodes;
        double x = starRadius * std::cos(angle);
        double y = starRadius * std::sin(angle);

        sensorMobility->SetPosition(Vector(x, y, 0.0));
    }

    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrWpanDevices = lrWpanHelper.Install(allNodes);

    InternetStackHelper internetStackHelper;
    internetStackHelper.Install(allNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ipInterfaces = ipv4.Assign(lrWpanDevices);

    Ipv4Address sinkIpAddress = ipInterfaces.GetAddress(0);

    uint16_t sinkPort = 9;
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(sinkNode);
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    OnOffHelper onoffHelper("ns3::UdpSocketFactory", InetSocketAddress(sinkIpAddress, sinkPort));
    onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // Keep OnTime for 1s, OffTime for 0s to make it always on
    onoffHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
    
    // Calculate rate to send one packet every 'packetInterval' seconds
    // Rate (bps) = (PacketSize * 8 bits/byte) / packetInterval (seconds)
    double rateBps = (double)(packetSize * 8) / packetInterval;
    onoffHelper.SetAttribute("DataRate", DataRateValue(DataRate(rateBps)));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numSensorNodes; ++i)
    {
        clientApps.Add(onoffHelper.Install(sensorNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0)); // Start client apps after sink is ready
    clientApps.Stop(Seconds(simulationTime));

    NetAnimHelper netanim;
    netanim.SetOutputFileName("star-topology.xml");
    netanim.SetConstantPositionNodes(allNodes);
    netanim.EnablePacketMetadata(true);

    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> monitor = flowMonitorHelper.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForCompletion();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalDelay = 0;
    double totalThroughput = 0;
    
    std::cout << "--- Per-Flow Statistics ---" << std::endl;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        // We are interested in flows from sensor nodes to the sink
        if (t.destinationAddress == sinkIpAddress)
        {
            totalTxPackets += i->second.txPackets;
            totalRxPackets += i->second.rxPackets;
            totalDelay += i->second.delaySum.GetSeconds();
            totalThroughput += i->second.rxBytes * 8.0 / (simulationTime * 1000000.0); // Convert to Mbps

            std::cout << "  Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
            std::cout << "    Tx Packets: " << i->second.txPackets << std::endl;
            std::cout << "    Rx Packets: " << i->second.rxPackets << std::endl;
            std::cout << "    Lost Packets: " << i->second.txPackets - i->second.rxPackets << std::endl;
            if (i->second.rxPackets > 0)
            {
                std::cout << "    Packet Delivery Ratio: " << (double)i->second.rxPackets / i->second.txPackets * 100.0 << "%" << std::endl;
                std::cout << "    Average Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << "s" << std::endl;
            }
            std::cout << "    Throughput: " << i->second.rxBytes * 8.0 / (simulationTime * 1000000.0) << " Mbps" << std::endl;
        }
    }

    std::cout << "\n--- Aggregate Statistics ---" << std::endl;
    std::cout << "Total Tx Packets: " << totalTxPackets << std::endl;
    std::cout << "Total Rx Packets: " << totalRxPackets << std::endl;
    std::cout << "Total Lost Packets: " << totalTxPackets - totalRxPackets << std::endl;
    if (totalTxPackets > 0)
    {
        std::cout << "Overall Packet Delivery Ratio: " << (totalRxPackets / totalTxPackets) * 100.0 << "%" << std::endl;
    }
    if (totalRxPackets > 0)
    {
        std::cout << "Overall Average Latency: " << totalDelay / totalRxPackets << "s" << std::endl;
    }
    std::cout << "Overall Throughput: " << totalThroughput << " Mbps" << std::endl;

    Simulator::Destroy();

    return 0;
}