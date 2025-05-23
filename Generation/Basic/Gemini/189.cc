#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-westwood-module.h" // Includes various TCP variants like TcpNewReno
#include "ns3/error-model.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // 1. Set up default values and command line arguments
    uint32_t numNodes = 4; // Fixed as per program description
    double simulationTime = 10.0; // seconds
    double errorRate = 0.0001; // Packet error rate for the CSMA channel
    std::string dataRate = "10Mbps";
    Time channelDelay = NanoSeconds(6560); // 6.56us delay (typical for 10Mbps Ethernet)

    // Configure TCP variant (e.g., TcpNewReno)
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    // Configure OnOffApplication packet size and data rate
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(1024)); // 1 KB packets
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("1Mbps")); // Each client sends at 1 Mbps

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("errorRate", "Packet error rate for the CSMA channel", errorRate);
    cmd.AddValue("dataRate", "CSMA channel data rate", dataRate);
    cmd.AddValue("channelDelay", "CSMA channel propagation delay (ns)", channelDelay);
    cmd.Parse(argc, argv);

    // 2. Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // 3. Configure CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue(dataRate));
    csma.SetChannelAttribute("Delay", TimeValue(channelDelay));

    NetDeviceContainer csmaDevices = csma.Install(nodes);

    // 4. Install Internet Stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // 5. Assign IP Addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ipv4Interfaces = ipv4.Assign(csmaDevices);

    // 6. Configure Packet Loss (Error Model) on CSMA devices
    // Apply a RateErrorModel to all CSMA devices to simulate packet loss on the shared medium.
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetRate(errorRate);
    em->SetRandomStream(uv);

    for (uint32_t i = 0; i < csmaDevices.GetN(); ++i)
    {
        csmaDevices.Get(i)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    }

    // 7. Setup TCP Applications (One server and three clients)
    // Node 0 will be the server. Nodes 1, 2, 3 will be clients.

    // Server setup (Node 0)
    uint16_t port = 9; // Discard port for PacketSink
    Address sinkAddress(InetSocketAddress(ipv4Interfaces.GetAddress(0), port)); // Server is node 0

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = sinkHelper.Install(nodes.Get(0)); // Install PacketSink on node 0
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime + 1.0)); // Run slightly longer than clients

    // Client setup (Nodes 1, 2, 3)
    OnOffHelper onoffHelper("ns3::TcpSocketFactory", Address()); // Address will be set for each client
    onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Always on
    onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Never off (continuous traffic)

    ApplicationContainer clientApps;
    // Iterate from node 1 to numNodes-1 (which is 3 in this case)
    for (uint32_t i = 1; i < numNodes; ++i)
    {
        onoffHelper.SetAttribute("Remote", AddressValue(sinkAddress)); // All clients send to the same server
        clientApps.Add(onoffHelper.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0)); // Start clients after 1 second
    clientApps.Stop(Seconds(simulationTime)); // Stop clients at simulationTime

    // 8. Enable NetAnim Tracing
    NetAnimHelper netanim;
    // Set explicit positions for better visualization in NetAnim
    netanim.SetNodeLocation2D(nodes.Get(0), 10.0, 50.0); // Server node
    netanim.SetNodeLocation2D(nodes.Get(1), 50.0, 10.0); // Client 1
    netanim.SetNodeLocation2D(nodes.Get(2), 50.0, 90.0); // Client 2
    netanim.SetNodeLocation2D(nodes.Get(3), 90.0, 50.0); // Client 3
    netanim.EnablePacketMetadata(true); // Show packet details in NetAnim
    netanim.EnableTracing(); // Generates the XML trace file (e.g., csma-network.xml)

    // 9. Enable FlowMonitor for metrics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // 10. Run Simulation
    Simulator::Stop(Seconds(simulationTime + 2.0)); // Stop a bit after applications end to collect full stats
    Simulator::Run();

    // 11. Gather and print metrics from FlowMonitor
    monitor->CheckFor	EachFlowStats (
    {
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.Get	};
        FlowMonitor::FlowStatsMap::const_iterator flowIt;
        for (flowIt = monitor->Get	FlowStats().begin(); flowIt != monitor->Get	FlowStats().end(); ++flowIt)
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->GetFlow(flowIt->first);

            // Filter for flows going to the server (Node 0's IP address)
            if (t.destinationAddress == ipv4Interfaces.GetAddress(0))
            {
                double bytesReceived = flowIt->second.rxBytes;
                double txPackets = flowIt->second.txPackets;
                double rxPackets = flowIt->second.rxPackets;
                // Calculate flow duration considering first transmitted and last received packet
                double flowDuration = flowIt->second.timeLastRxPacket.GetSeconds() - flowIt->second.timeFirstTxPacket.GetSeconds();
                if (flowDuration <= 0.0) // Avoid division by zero if flow hasn't properly started/ended or no packets received
                {
                    flowDuration = simulationTime; // Default to simulation time if duration is invalid
                }

                // Throughput in Mbps
                double throughput = (bytesReceived * 8.0) / (flowDuration * 1000000.0);

                // Packet Loss
                uint64_t packetsLost = txPackets - rxPackets;
                double packetLossRatio = (txPackets > 0) ? (double)packetsLost / txPackets : 0.0;

                // Average Delay (end-to-end delay)
                double averageDelay = 0.0;
                if (flowIt->second.rxPackets > 0)
                {
                    averageDelay = flowIt->second.delaySum.GetSeconds() / flowIt->second.rxPackets;
                }

                std::cout << "Flow " << flowIt->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
                std::cout << "  Protocol: " << (uint16_t)t.protocol << std::endl;
                std::cout << "  Throughput: " << throughput << " Mbps" << std::endl;
                std::cout << "  Packet Loss: " << packetsLost << " packets (" << (packetLossRatio * 100.0) << " %)" << std::endl;
                std::cout << "  Average Delay: " << averageDelay * 1000.0 << " ms" << std::endl;
                std::cout << "  Total Transmitted Packets: " << txPackets << std::endl;
                std::cout << "  Total Received Packets: " << rxPackets << std::endl;
                std::cout << std::endl;
            }
        }
    });

    // 12. Cleanup
    Simulator::Destroy();

    return 0;
}