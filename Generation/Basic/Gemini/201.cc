#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <string>
#include <iostream>

int main(int argc, char *argv[])
{
    // Simulation parameters
    double simulationTime = 10.0; // seconds
    uint16_t udpEchoPort = 9;

    // Create nodes
    ns3::NodeContainer allNodes;
    allNodes.Create(5); // Node 0: central, Nodes 1-4: peripheral

    ns3::Ptr<ns3::Node> centralNode = allNodes.Get(0);
    ns3::NodeContainer peripheralNodes;
    for (uint32_t i = 1; i <= 4; ++i)
    {
        peripheralNodes.Add(allNodes.Get(i));
    }

    // Create point-to-point links
    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue("1ms"));

    ns3::NetDeviceContainer deviceContainers[4];       // Array to hold NetDeviceContainer for each link
    ns3::Ipv4InterfaceContainer interfaceContainers[4]; // Array to hold Ipv4InterfaceContainer for each link

    ns3::Ipv4AddressHelper ipv4;

    for (uint32_t i = 0; i < 4; ++i)
    {
        ns3::NodeContainer linkNodes;
        linkNodes.Add(centralNode);
        linkNodes.Add(peripheralNodes.Get(i));

        deviceContainers[i] = p2pHelper.Install(linkNodes);

        std::string subnet = "10.1." + std::to_string(i + 1) + ".0";
        ipv4.SetBase(subnet.c_str(), "255.255.255.0");
        interfaceContainers[i] = ipv4.Assign(deviceContainers[i]);
    }

    // Install InternetStack on all nodes
    ns3::InternetStackHelper stack;
    stack.Install(allNodes);

    // Install UDP Echo Server on central node
    ns3::UdpEchoServerHelper echoServer(udpEchoPort);
    ns3::ApplicationContainer serverApps = echoServer.Install(centralNode);
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(simulationTime));

    // Install UDP Echo Clients on peripheral nodes
    ns3::ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 4; ++i)
    {
        // Each client sends to the central node's IP address on its respective link
        // interfaceContainers[i].GetAddress(0) is the central node's IP on link 'i'
        ns3::UdpEchoClientHelper echoClient(interfaceContainers[i].GetAddress(0), udpEchoPort);
        echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(100));
        echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(0.1)));
        echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024));
        clientApps.Add(echoClient.Install(peripheralNodes.Get(i)));
    }

    clientApps.Start(ns3::Seconds(2.0));
    clientApps.Stop(ns3::Seconds(simulationTime - 0.5));

    // Enable pcap tracing for all point-to-point devices
    for (uint32_t i = 0; i < 4; ++i)
    {
        p2pHelper.EnablePcap("star-topology", deviceContainers[i]);
    }

    // Configure FlowMonitor
    ns3::FlowMonitorHelper flowMonitorHelper;
    ns3::Ptr<ns3::FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    // Set simulation stop time and run
    ns3::Simulator::Stop(ns3::Seconds(simulationTime));
    ns3::Simulator::Run();

    // Collect and log metrics using FlowMonitor
    flowMonitor->CheckFor_Flows();
    ns3::Ptr<ns3::Ipv4FlowClassifier> classifier = ns3::DynamicCast<ns3::Ipv4FlowClassifier>(flowMonitorHelper.Get_FlowClassifier());
    std::map<ns3::FlowId, ns3::FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    double totalAvgDelay = 0.0;
    double totalAvgThroughput = 0.0;
    double totalAvgDeliveryRatio = 0.0;
    uint32_t clientFlowCount = 0;

    for (std::map<ns3::FlowId, ns3::FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        ns3::Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        // Filter for UDP Echo client-to-server flows (destination port is the echo server port, protocol is UDP)
        if (t.destinationPort == udpEchoPort && t.protocol == 17) // 17 is UDP protocol number
        {
            std::cout << "--- Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
            std::cout << "  Tx Packets: " << i->second.txPackets << std::endl;
            std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
            std::cout << "  Tx Bytes:   " << i->second.txBytes << std::endl;
            std::cout << "  Rx Bytes:   " << i->second.rxBytes << std::endl;

            double deliveryRatio = 0.0;
            if (i->second.txPackets > 0)
            {
                deliveryRatio = (double)i->second.rxPackets / i->second.txPackets * 100.0;
            }
            std::cout << "  Delivery Ratio: " << deliveryRatio << " %" << std::endl;

            double avgFlowDelayMs = 0.0;
            if (i->second.rxPackets > 0)
            {
                avgFlowDelayMs = i->second.delaySum.GetMilliSeconds() / i->second.rxPackets;
            }
            std::cout << "  Avg Delay: " << avgFlowDelayMs << " ms" << std::endl;

            double throughputMbps = 0.0;
            if (i->second.rxBytes > 0)
            {
                // Calculate throughput for the active duration of the flow
                double flowDuration = i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds();
                if (flowDuration > 0)
                {
                    throughputMbps = (i->second.rxBytes * 8.0) / flowDuration / 1024 / 1024;
                }
            }
            std::cout << "  Throughput: " << throughputMbps << " Mbps" << std::endl;

            totalAvgDelay += avgFlowDelayMs;
            totalAvgThroughput += throughputMbps;
            totalAvgDeliveryRatio += deliveryRatio;
            clientFlowCount++;
        }
    }

    if (clientFlowCount > 0)
    {
        std::cout << "\n--- Overall Averages for Client-Server Flows ---" << std::endl;
        std::cout << "  Average Packet Delivery Ratio: " << totalAvgDeliveryRatio / clientFlowCount << " %" << std::endl;
        std::cout << "  Average Delay: " << totalAvgDelay / clientFlowCount << " ms" << std::endl;
        std::cout << "  Average Throughput: " << totalAvgThroughput / clientFlowCount << " Mbps" << std::endl;
    }

    // Clean up
    ns3::Simulator::Destroy();

    return 0;
}