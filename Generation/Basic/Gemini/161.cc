#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <string>
#include <map>
#include <iomanip>

using namespace ns3;

int main(int argc, char *argv[])
{
    // Configure simulation parameters
    double simTime = 10.0; // seconds
    std::string appDataRate = "1Mbps";
    std::string p2pDataRate = "100Mbps";
    std::string p2pDelay = "10ms";

    // Allow user to override simulation parameters via command line
    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total simulation time in seconds", simTime);
    cmd.AddValue("appDataRate", "Application data rate (e.g., 1Mbps)", appDataRate);
    cmd.AddValue("p2pDataRate", "Point-to-point link data rate (e.g., 100Mbps)", p2pDataRate);
    cmd.AddValue("p2pDelay", "Point-to-point link delay (e.g., 10ms)", p2pDelay);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create Point-to-Point channel
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(p2pDataRate));
    p2p.SetChannelAttribute("Delay", StringValue(p2pDelay));

    NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup TCP Application
    uint16_t tcpPort = 9;
    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install(nodes.Get(1));
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(Seconds(simTime));

    OnOffHelper tcpClientHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(interfaces.GetAddress(1), tcpPort));
    tcpClientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    tcpClientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    tcpClientHelper.SetAttribute("DataRate", DataRateValue(DataRate(appDataRate)));
    tcpClientHelper.SetAttribute("PacketSize", UintegerValue(1024)); // Bytes
    ApplicationContainer tcpClientApp = tcpClientHelper.Install(nodes.Get(0));
    double appStartTime = 1.0;
    double appStopTime = simTime - 1.0; // Stop 1 second before end to allow for connection teardown
    tcpClientApp.Start(Seconds(appStartTime));
    tcpClientApp.Stop(Seconds(appStopTime));

    // Setup UDP Application
    uint16_t udpPort = 10;
    PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer udpSinkApp = udpSinkHelper.Install(nodes.Get(1));
    udpSinkApp.Start(Seconds(0.0));
    udpSinkApp.Stop(Seconds(simTime));

    OnOffHelper udpClientHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(interfaces.GetAddress(1), udpPort));
    udpClientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    udpClientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    udpClientHelper.SetAttribute("DataRate", DataRateValue(DataRate(appDataRate)));
    udpClientHelper.SetAttribute("PacketSize", UintegerValue(1024)); // Bytes
    ApplicationContainer udpClientApp = udpClientHelper.Install(nodes.Get(0));
    udpClientApp.Start(Seconds(appStartTime));
    udpClientApp.Stop(Seconds(appStopTime));

    // Enable NetAnim tracing
    AnimationInterface anim("tcp-udp-comparison.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 50.0); // Node 0 at (10, 50)
    anim.SetConstantPosition(nodes.Get(1), 90.0, 50.0); // Node 1 at (90, 50)

    // Setup FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Analyze FlowMonitor results
    monitor->CheckForChanges();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsCollection stats = monitor->GetStats();

    double tcpTotalRxBytes = 0.0;
    double tcpTotalDelaySeconds = 0.0;
    uint64_t tcpTotalRxPackets = 0;
    uint64_t tcpTotalTxPackets = 0;

    double udpTotalRxBytes = 0.0;
    double udpTotalDelaySeconds = 0.0;
    uint64_t udpTotalRxPackets = 0;
    uint64_t udpTotalTxPackets = 0;

    double appActiveDuration = appStopTime - appStartTime;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        // Only interested in flows from Node 0 (client) to Node 1 (server)
        if (t.sourceAddress == interfaces.GetAddress(0) && t.destinationAddress == interfaces.GetAddress(1))
        {
            if (t.protocol == 6) // TCP (protocol number for TCP)
            {
                tcpTotalRxBytes += i->second.rxBytes;
                tcpTotalDelaySeconds += i->second.delaySum.GetSeconds();
                tcpTotalRxPackets += i->second.rxPackets;
                tcpTotalTxPackets += i->second.txPackets;
            }
            else if (t.protocol == 17) // UDP (protocol number for UDP)
            {
                udpTotalRxBytes += i->second.rxBytes;
                udpTotalDelaySeconds += i->second.delaySum.GetSeconds();
                udpTotalRxPackets += i->second.rxPackets;
                udpTotalTxPackets += i->second.txPackets;
            }
        }
    }

    // Calculate overall metrics for printing
    double finalTcpThroughput = 0.0;
    if (appActiveDuration > 0)
    {
        finalTcpThroughput = (tcpTotalRxBytes * 8.0) / appActiveDuration / 1000000.0; // Mbps
    }
    double finalTcpLatency = 0.0;
    if (tcpTotalRxPackets > 0)
    {
        finalTcpLatency = (tcpTotalDelaySeconds / tcpTotalRxPackets) * 1000.0; // ms
    }
    double finalTcpPdr = 0.0;
    if (tcpTotalTxPackets > 0)
    {
        finalTcpPdr = (double)tcpTotalRxPackets * 100.0 / tcpTotalTxPackets; // %
    }

    double finalUdpThroughput = 0.0;
    if (appActiveDuration > 0)
    {
        finalUdpThroughput = (udpTotalRxBytes * 8.0) / appActiveDuration / 1000000.0; // Mbps
    }
    double finalUdpLatency = 0.0;
    if (udpTotalRxPackets > 0)
    {
        finalUdpLatency = (udpTotalDelaySeconds / udpTotalRxPackets) * 1000.0; // ms
    }
    double finalUdpPdr = 0.0;
    if (udpTotalTxPackets > 0)
    {
        finalUdpPdr = (double)udpTotalRxPackets * 100.0 / udpTotalTxPackets; // %
    }
    
    // Print results
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "\n--- Simulation Results ---\n";
    std::cout << "Simulation Time: " << simTime << " seconds\n";
    std::cout << "Application Active Duration: " << appActiveDuration << " seconds\n";
    std::cout << "Application Data Rate: " << appDataRate << "\n";
    std::cout << "Point-to-Point Link: " << p2pDataRate << ", " << p2pDelay << "\n\n";

    std::cout << "TCP Performance:\n";
    if (tcpTotalTxPackets > 0)
    {
        std::cout << "  Throughput: " << finalTcpThroughput << " Mbps\n";
        std::cout << "  Latency: " << finalTcpLatency << " ms\n";
        std::cout << "  Packet Delivery Ratio: " << finalTcpPdr << " %\n";
    }
    else
    {
        std::cout << "  No TCP traffic data available or no packets transmitted.\n";
    }

    std::cout << "\nUDP Performance:\n";
    if (udpTotalTxPackets > 0)
    {
        std::cout << "  Throughput: " << finalUdpThroughput << " Mbps\n";
        std::cout << "  Latency: " << finalUdpLatency << " ms\n";
        std::cout << "  Packet Delivery Ratio: " << finalUdpPdr << " %\n";
    }
    else
    {
        std::cout << "  No UDP traffic data available or no packets transmitted.\n";
    }

    // Cleanup
    Simulator::Destroy();

    return 0;
}