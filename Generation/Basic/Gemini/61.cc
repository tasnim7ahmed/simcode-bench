#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/tcp-socket.h" // For TcpSocket
#include "ns3/queue.h" // For Ptr<Queue> and Queue<Packet>

#include <string>
#include <fstream>
#include <map>
#include <algorithm> // For std::replace

// Namespace for ns-3
using namespace ns3;

// Global file streams for tracing
std::ofstream g_cwndFile;
std::ofstream g_queueSizeFile;
std::ofstream g_queueDropFile;
std::ofstream g_queueStatsFile; // For FlowMonitor summary

// Map to keep track of current queue sizes per device context path
std::map<std::string, uint32_t> g_currentQueueSize;

// Tracing function for Congestion Window
void CwndTracer(uint32_t oldCwnd, uint32_t newCwnd)
{
    g_cwndFile << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

// Tracing function for Queue Enqueue events
void EnqueueTrace(std::string context, Ptr<const QueueDiscItem> item)
{
    // context format: /NodeList/X/DeviceList/Y/$ns3::PointToPointNetDevice/TxQueue
    g_currentQueueSize[context]++;
    g_queueSizeFile << Simulator::Now().GetSeconds() << "\t" << context << "\t" << g_currentQueueSize[context] << std::endl;
}

// Tracing function for Queue Dequeue events
void DequeueTrace(std::string context, Ptr<const QueueDiscItem> item)
{
    // context format: /NodeList/X/DeviceList/Y/$ns3::PointToPointNetDevice/TxQueue
    if (g_currentQueueSize[context] > 0)
    {
        g_currentQueueSize[context]--;
    }
    else
    {
        // This case indicates a potential issue or synchronization quirk, but should not cause negative size.
        NS_LOG_WARN("Dequeue called on empty queue context: " << context);
    }
    g_queueSizeFile << Simulator::Now().GetSeconds() << "\t" << context << "\t" << g_currentQueueSize[context] << std::endl;
}

// Tracing function for Queue Drop events
void DropTrace(std::string context, Ptr<const QueueDiscItem> item)
{
    // context format: /NodeList/X/DeviceList/Y/$ns3::PointToPointNetDevice/TxQueue
    // For DropTailQueue, a drop implies the packet was *not* enqueued, so queue size doesn't change due to drop itself.
    g_queueDropFile << Simulator::Now().GetSeconds() << "\t" << context << "\t" << "DROP" << std::endl;
}

int main(int argc, char *argv[])
{
    // Set up results directory
    std::string resultsDir = "results/";
    // Create the results directory and pcap subdirectory
    system(("mkdir -p " + resultsDir + "pcap").c_str());

    // Open trace files
    g_cwndFile.open(resultsDir + "cwndTraces");
    g_queueSizeFile.open(resultsDir + "queue-size.dat");
    g_queueDropFile.open(resultsDir + "queueTraces");
    g_queueStatsFile.open(resultsDir + "queueStats.txt");

    // Configure TCP variant (e.g., NewReno)
    // ns3::TcpNewReno is the default, but explicitly setting it is good practice
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    
    // Create four nodes
    NodeContainer nodes;
    nodes.Create(4); // n0, n1, n2, n3

    // Create PointToPointHelper for links
    PointToPointHelper p2p;

    // Link n0-n1: 10 Mbps, 1 ms delay
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer d0d1 = p2p.Install(nodes.Get(0), nodes.Get(1));

    // Link n1-n2: 1 Mbps, 10 ms delay
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer d1d2 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Link n2-n3: 10 Mbps, 1 ms delay
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer d2d3 = p2p.Install(nodes.Get(2), nodes.Get(3));

    // Install Internet Stack on all nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign(d2d3);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // TCP Flow from n0 to n3 using BulkSendApplication
    uint16_t sinkPort = 9000; // Use a distinct port

    // PacketSink on n3
    // Use the address of n3 on the link to n2 (i2i3.GetAddress(1))
    Address sinkAddress(InetSocketAddress(i2i3.GetAddress(1), sinkPort)); 
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::Any, sinkPort));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // BulkSendApplication on n0
    BulkSendHelper sourceHelper("ns3::TcpSocketFactory", sinkAddress);
    sourceHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send until simulation ends
    sourceHelper.SetAttribute("SendBufferSize", UintegerValue(1048576)); // 1 MB buffer
    ApplicationContainer sourceApp = sourceHelper.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.0)); // Start after a small delay
    sourceApp.Stop(Seconds(9.0));  // Stop before simulation ends to allow final packets to clear

    // Enable PCAP tracing for all PointToPoint devices installed by this helper
    p2p.EnablePcapAll(resultsDir + "pcap"); 

    // Tracing setup
    // Cwnd trace for the BulkSendApplication's TCP socket on n0
    // The path for the BulkSendApplication's socket is /NodeList/0/ApplicationList/0/$ns3::BulkSendApplication/Socket
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::BulkSendApplication/Socket/CongestionWindow",
                                  MakeCallback(&CwndTracer));

    // Queue traces (length and drops) for all PointToPointNetDevice TxQueues
    // Iterate through all nodes and their devices to connect to queue trace sources.
    // PointToPointNetDevice has a single TxQueue.
    for (uint32_t i = 0; i < nodes.GetN(); ++i) // Iterate through nodes
    {
        for (uint32_t j = 0; j < nodes.Get(i)->GetNDevices(); ++j) // Iterate through devices on each node
        {
            Ptr<NetDevice> device = nodes.Get(i)->GetDevice(j);
            Ptr<PointToPointNetDevice> p2pDevice = DynamicCast<PointToPointNetDevice>(device);
            if (p2pDevice)
            {
                // Get the TxQueue of the PointToPointNetDevice
                Ptr<Queue<Packet>> queue = p2pDevice->GetQueue(); 
                if (queue)
                {
                    // Construct the full trace source path for this specific queue
                    std::string queuePath = "/NodeList/" + std::to_string(i) + "/DeviceList/" + std::to_string(j) + "/$ns3::PointToPointNetDevice/TxQueue";
                    g_currentQueueSize[queuePath] = 0; // Initialize queue size for this queue

                    // Connect trace sinks
                    queue->TraceConnect("Enqueue", queuePath, MakeCallback(&EnqueueTrace));
                    queue->TraceConnect("Dequeue", queuePath, MakeCallback(&DequeueTrace));
                    queue->TraceConnect("Drop", queuePath, MakeCallback(&DropTrace));
                }
            }
        }
    }

    // Print global configuration to file
    std::string configFilePath = resultsDir + "config.txt";
    std::ofstream configFstream;
    configFstream.open(configFilePath.c_str());
    Config::PrintGlobalConfig(configFstream);
    configFstream.close();

    // Flow Monitor for overall statistics
    Ptr<FlowMonitor> flowMon;
    FlowMonitorHelper flowHelper;
    flowMon = flowHelper.InstallAll();

    // Set simulation duration
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Flow Monitor statistics output
    flowMon->CheckForEvents(); // Recalculate stats up to current simulation time
    flowMon->SerializeToXmlFile(resultsDir + "flow_monitor_output.xml", true, true);

    // Process FlowMonitor results for "queueStats.txt" (summarizing flows)
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

    g_queueStatsFile << "--- Flow Monitor Statistics Summary ---" << std::endl;
    if (classifier)
    {
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowMon->GetFlowStats().begin();
             i != flowMon->GetFlowStats().end(); ++i)
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
            g_queueStatsFile << "Flow " << i->first << ": " << t.sourceAddress << ":" << t.sourcePort << " -> " << t.destinationAddress << ":" << t.destinationPort << std::endl;
            g_queueStatsFile << "  Tx Packets: " << i->second.txPackets << std::endl;
            g_queueStatsFile << "  Rx Packets: " << i->second.rxPackets << std::endl;
            g_queueStatsFile << "  Lost Packets: " << i->second.lostPackets << " (includes drops due to full queues)" << std::endl;
            if (i->second.txPackets > 0)
            {
                g_queueStatsFile << "  Packet Loss Ratio: " << (double)i->second.lostPackets / i->second.txPackets << std::endl;
            }
            if (i->second.timeLastRxPacket.IsPositive() && i->second.timeFirstTxPacket.IsPositive() &&
                i->second.timeLastRxPacket.GetSeconds() > i->second.timeFirstTxPacket.GetSeconds())
            {
                 g_queueStatsFile << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps" << std::endl;
            }
            if (i->second.rxPackets > 0)
            {
                g_queueStatsFile << "  Mean Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s" << std::endl;
                g_queueStatsFile << "  Mean Jitter: " << i->second.jitterSum.GetSeconds() / i->second.rxPackets << " s" << std::endl;
            }
        }
    }
    g_queueStatsFile << "-------------------------------------" << std::endl;

    Simulator::Destroy();

    // Close trace files
    g_cwndFile.close();
    g_queueSizeFile.close();
    g_queueDropFile.close();
    g_queueStatsFile.close();

    return 0;
}