#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/queue-disc-module.h"
#include "ns3/red-queue-disc.h"
#include "ns3/nlred-queue-disc.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"

#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellRedSimulation");

// Global Ptr to store the queue disc for statistics collection
Ptr<QueueDisc> g_queueDisc = nullptr;

// Function to print queue disc statistics
void
PrintQueueDiscStats()
{
    if (g_queueDisc == nullptr)
    {
        NS_LOG_WARN("QueueDisc pointer is null, cannot print stats.");
        return;
    }

    QueueDiscStats stats = g_queueDisc->GetStats();

    Time now = Simulator::Now();
    std::cout << now.GetSeconds() << "s - Queue Disc Stats (Total):" << std::endl;
    std::cout << "  Packets Enqueued: " << stats.GetPacketsEnqueued() << std::endl;
    std::cout << "  Packets Dropped: " << stats.GetPacketsDropped() << std::endl;
    std::cout << "  Bytes Enqueued: " << stats.GetBytesEnqueued() << std::endl;
    std::cout << "  Bytes Dropped: " << stats.GetBytesDropped() << std::endl;
    std::cout << "  Packets Marked: " << stats.GetPacketsMarked() << std::endl;
    std::cout << "  Bytes Marked: " << stats.GetBytesMarked() << std::endl;
    std::cout << "  Current Queue Size (packets): " << g_queueDisc->GetCurrentSize().GetPackets() << std::endl;
    std::cout << "  Current Queue Size (bytes): " << g_queueDisc->GetCurrentSize().GetBytes() << std::endl;

    // Schedule next call
    Simulator::Schedule(Seconds(1.0), &PrintQueueDiscStats);
}

int
main(int argc, char* argv[])
{
    // 0. Default values for parameters
    uint32_t nLeaf = 2;
    uint32_t queueDiscLimitPackets = 1000;
    uint32_t packetSize = 1024; // bytes
    std::string dataRate = "1Mbps";
    std::string linkDataRate = "10Mbps";
    std::string linkDelay = "10ms";
    double redMinTh = 5.0;
    double redMaxTh = 15.0;
    bool redRandMode = true; // True for byte-based, false for packet-based randomization (RED's Mode attribute)
    std::string redQueueDiscType = "Red"; // "Red" or "NlRed"
    bool byteMode = false; // True for byte-based queue limit, false for packet-based

    // 1. Command line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("nLeaf", "Number of leaf nodes per side", nLeaf);
    cmd.AddValue("queueDiscLimitPackets", "Queue disc limit in packets", queueDiscLimitPackets);
    cmd.AddValue("packetSize", "Size of data packets in bytes", packetSize);
    cmd.AddValue("dataRate", "Application data rate", dataRate);
    cmd.AddValue("linkDataRate", "Link data rate", linkDataRate);
    cmd.AddValue("linkDelay", "Link delay", linkDelay);
    cmd.AddValue("redMinTh", "RED min threshold", redMinTh);
    cmd.AddValue("redMaxTh", "RED max threshold", redMaxTh);
    cmd.AddValue("redRandMode", "RED random mode (true for byte-based, false for packet-based randomization)", redRandMode);
    cmd.AddValue("redQueueDiscType", "RED Queue Disc Type (Red or NlRed)", redQueueDiscType);
    cmd.AddValue("byteMode", "Queue Disc Mode (true for byte-based, false for packet-based limits)", byteMode);
    cmd.Parse(argc, argv);

    // 2. Set up logging (optional, but good for debugging)
    LogComponentEnable("DumbbellRedSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("RedQueueDisc", LOG_LEVEL_INFO);
    LogComponentEnable("NlRedQueueDisc", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // 3. Configure P2P links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(linkDataRate));
    p2p.SetChannelAttribute("Delay", StringValue(linkDelay));

    // 4. Configure QueueDisc limits and RED parameters
    QueueDiscLimitParameters limit;
    limit.SetType(byteMode ? QueueDiscLimit::Mode::QUEUE_DISC_LIMIT_BYTES : QueueDiscLimit::Mode::QUEUE_DISC_LIMIT_PACKETS);
    limit.SetLimit(byteMode ? (queueDiscLimitPackets * packetSize) : queueDiscLimitPackets);

    if (redQueueDiscType == "Red")
    {
        Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(redMinTh));
        Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(redMaxTh));
        Config::SetDefault("ns3::RedQueueDisc::QW", DoubleValue(0.002)); // Weight factor for average queue size
        Config::SetDefault("ns3::RedQueueDisc::RedLinkStandby", BooleanValue(false));
        Config::SetDefault("ns3::RedQueueDisc::LinkBandwidth", StringValue(linkDataRate));
        Config::SetDefault("ns3::RedQueueDisc::Mode", EnumValue(redRandMode ? RedQueueDisc::AQM_RED_MODE_BYTE : RedQueueDisc::AQM_RED_MODE_PACKET));
        Config::SetDefault("ns3::RedQueueDisc::QueueDiscLimit", QueueDiscLimitValue(limit));
    }
    else if (redQueueDiscType == "NlRed")
    {
        Config::SetDefault("ns3::NlRedQueueDisc::MinTh", DoubleValue(redMinTh));
        Config::SetDefault("ns3::NlRedQueueDisc::MaxTh", DoubleValue(redMaxTh));
        Config::SetDefault("ns3::NlRedQueueDisc::QW", DoubleValue(0.002));
        Config::SetDefault("ns3::NlRedQueueDisc::K", DoubleValue(0.001)); // K factor for NLRED
        Config::SetDefault("ns3::NlRedQueueDisc::Beta", DoubleValue(1.0)); // Beta factor for NLRED
        Config::SetDefault("ns3::NlRedQueueDisc::LinkBandwidth", StringValue(linkDataRate));
        Config::SetDefault("ns3::NlRedQueueDisc::Mode", EnumValue(redRandMode ? NlRedQueueDisc::AQM_NLRED_MODE_BYTE : NlRedQueueDisc::AQM_NLRED_MODE_PACKET));
        Config::SetDefault("ns3::NlRedQueueDisc::QueueDiscLimit", QueueDiscLimitValue(limit));
    }
    else
    {
        NS_FATAL_ERROR("Unknown Queue Disc Type: " << redQueueDiscType << ". Must be 'Red' or 'NlRed'.");
    }

    // 5. Create nodes
    NodeContainer coreNodes;
    coreNodes.Create(2); // N0 and N1

    NodeContainer leftLeafNodes;
    leftLeafNodes.Create(nLeaf);

    NodeContainer rightLeafNodes;
    rightLeafNodes.Create(nLeaf);

    // 6. Install internet stack
    InternetStackHelper stack;
    stack.Install(coreNodes);
    stack.Install(leftLeafNodes);
    stack.Install(rightLeafNodes);

    // 7. Create device containers for core-to-leaf links and core-to-core link
    NetDeviceContainer coreLeftDevices;
    NetDeviceContainer coreRightDevices;
    NetDeviceContainer coreCoreDevices;

    // 8. Connect core nodes to left leaf nodes (N0 to left leaves)
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NetDeviceContainer devices = p2p.Install(coreNodes.Get(0), leftLeafNodes.Get(i));
        coreLeftDevices.Add(devices);
    }

    // 9. Connect core nodes to right leaf nodes (N1 to right leaves)
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NetDeviceContainer devices = p2p.Install(coreNodes.Get(1), rightLeafNodes.Get(i));
        coreRightDevices.Add(devices);
    }

    // 10. Connect core nodes (bottleneck link N0-N1)
    coreCoreDevices = p2p.Install(coreNodes.Get(0), coreNodes.Get(1));

    // 11. Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");
    std::vector<Ipv4InterfaceContainer> leftInterfaces(nLeaf);
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        leftInterfaces[i] = address.Assign(coreLeftDevices.Get(i));
        address.NewNetwork(); // Increment network for each left-side link
    }

    address.SetBase("10.2.0.0", "255.255.255.0");
    std::vector<Ipv4InterfaceContainer> rightInterfaces(nLeaf);
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        rightInterfaces[i] = address.Assign(coreRightDevices.Get(i));
        address.NewNetwork(); // Increment network for each right-side link
    }

    address.SetBase("10.3.0.0", "255.255.255.0");
    Ipv4InterfaceContainer coreCoreInterfaces = address.Assign(coreCoreDevices);

    // 12. Install Queue Discs on bottleneck link (core-to-core)
    TrafficControlHelper tch;
    tch.SetRootQueueDisc(redQueueDiscType + "QueueDisc");
    tch.Install(coreCoreDevices);

    // Get a pointer to the installed queue disc for statistics.
    // We monitor the queue disc on the N0 side of the core-core link.
    g_queueDisc = tch.Get<QueueDisc>(coreCoreDevices.Get(0));
    if (g_queueDisc == nullptr)
    {
        NS_FATAL_ERROR("Failed to get QueueDisc pointer for statistics from coreCoreDevices.Get(0).");
    }

    // 13. Install applications
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        // Server on left leaf node
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), 9));
        serverApps.Add(sinkHelper.Install(leftLeafNodes.Get(i)));

        // Client on right leaf node
        OnOffHelper onoff("ns3::TcpSocketFactory",
                          InetSocketAddress(leftInterfaces[i].GetAddress(1), 9));
        // Random On/Off periods (Exponential distribution)
        onoff.SetAttribute("OnTime", StringValue("ns3::ExponentialRandomVariable[Mean=1.0|Bound=10.0]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=1.0|Bound=10.0]"));
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
        clientApps.Add(onoff.Install(rightLeafNodes.Get(i)));
    }

    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));
    clientApps.Start(Seconds(1.0)); // Start clients after servers are up
    clientApps.Stop(Seconds(9.0));

    // 14. Schedule statistics printing
    Simulator::Schedule(Seconds(1.0), &PrintQueueDiscStats);

    // 15. Simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}