#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot-helper.h"
#include "ns3/netanim-module.h"

#include <iostream>
#include <fstream>
#include <string>
#include <map>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("MyWiFiMultiRateExperiment");

/**
 * \brief A class to encapsulate the WiFi multirate experiment logic.
 *
 * This class sets up a WiFi ad-hoc network with a specified number of nodes,
 * implements a multirate configuration by assigning different rate managers
 * (AARF and ConstantRate) to subsets of nodes, includes node mobility,
 * multiple UDP OnOff applications, AODV routing, and collects results
 * using FlowMonitor and Gnuplot.
 */
class MyWiFiMultiRateExperiment
{
public:
    MyWiFiMultiRateExperiment();
    void Run();

private:
    // Simulation parameters
    uint32_t m_numNodes;          ///< Number of nodes in the simulation
    double m_simTime;             ///< Simulation time in seconds
    uint32_t m_numFlows;          ///< Number of UDP OnOff application flows
    std::string m_phyModeAarf;    ///< Default starting Phy Mode for AARF
    std::string m_phyModeFixedLow; ///< Fixed low rate for a subset of nodes
    std::string m_phyModeFixedHigh; ///< Fixed high rate for a subset of nodes
    double m_txPower;             ///< WiFi Tx Power in dBm
    double m_mobilitySpeed;       ///< RandomWalk2dMobilityModel speed in m/s
    double m_mobilityPause;       ///< RandomWalk2dMobilityModel pause time in seconds
    double m_arenaX;              ///< Mobility arena X dimension
    double m_arenaY;              ///< Mobility arena Y dimension
    bool m_pcapTracing;           ///< Enable PCAP tracing
    bool m_flowMonitor;           ///< Enable FlowMonitor
    bool m_netanimTracing;        ///< Enable NetAnim tracing
    uint32_t m_seed;              ///< RNG seed for reproducibility

    // ns-3 containers
    NodeContainer m_nodes;
    NetDeviceContainer m_devices;
    Ipv4InterfaceContainer m_interfaces;

    // Flow Monitor
    Ptr<FlowMonitor> m_flowMonitorPtr;
    FlowMonitorHelper m_flowMonitorHelper;

    // Gnuplot
    std::string m_plotFileName;       ///< Base name for Gnuplot output files
    GnuplotHelper m_plotHelper;       ///< Gnuplot helper for plotting
    Gnuplot2dPlot m_throughputPlot;   ///< Throughput plot configuration
    std::ofstream m_throughputDataFile; ///< Output file for throughput data

    // Throughput monitoring
    uint64_t m_totalRxBytes;          ///< Total bytes received across all nodes
    double m_lastThroughputCheckTime; ///< Last time throughput was checked

    // Private methods
    void ConfigureCommandLine();
    void SetupLogging();
    void SetupNodes();
    void SetupWifi();
    void SetupMobility();
    void SetupRouting(); // Placeholder, AODV set in InternetStackHelper
    void SetupApplications();
    void MonitorThroughput();
    void ReportResults();
    void CheckRx(Ptr<const Packet> p, const Address& addr); // Trace sink for MacRx
};

MyWiFiMultiRateExperiment::MyWiFiMultiRateExperiment()
    : m_numNodes(100),
      m_simTime(120.0), // Increased simulation time for more mobility and data
      m_numFlows(50),
      m_phyModeAarf("OfdmRate54Mbps"),
      m_phyModeFixedLow("OfdmRate6Mbps"),
      m_phyModeFixedHigh("OfdmRate54Mbps"),
      m_txPower(20.0),
      m_mobilitySpeed(15.0), // Slightly faster mobility
      m_mobilityPause(0.5), // Shorter pause for more dynamic movement
      m_arenaX(700.0),    // Larger arena for more dispersion
      m_arenaY(700.0),
      m_pcapTracing(false),
      m_flowMonitor(true),
      m_netanimTracing(false),
      m_seed(1),
      m_totalRxBytes(0),
      m_lastThroughputCheckTime(0.0)
{
    ConfigureCommandLine();
    SetupLogging();
    RngSeedManager::SetSeed(m_seed);

    // Initialize Gnuplot objects
    m_plotFileName = "wifi-multirate-throughput"; // .plt and .png will be appended
    m_throughputPlot.SetTitle("Aggregate Throughput vs. Time");
    m_throughputPlot.SetXAxisLabel("Time (s)");
    m_throughputPlot.SetYAxisLabel("Throughput (Mbps)");
    m_throughputPlot.AddLine(
        Gnuplot2dDataset(m_plotFileName + ".dat",
                         Gnuplot2dDataset::DATA_X_COLUMN_0,
                         Gnuplot2dDataset::DATA_Y_COLUMN_1)
            .SetTitle("Aggregate Throughput")
            .SetStyle(Gnuplot2dDataset::LINES_POINTS)
    );
    // Prepare data file for Gnuplot
    m_throughputDataFile.open(m_plotFileName + ".dat");
    m_throughputDataFile << "# Time (s)\tThroughput (Mbps)\n"; // Header for Gnuplot data file
}

void MyWiFiMultiRateExperiment::ConfigureCommandLine()
{
    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes", m_numNodes);
    cmd.AddValue("simTime", "Simulation time in seconds", m_simTime);
    cmd.AddValue("numFlows", "Number of UDP OnOff flows", m_numFlows);
    cmd.AddValue("phyModeAarf", "Default WiFi Phy Mode for AARF (e.g., OfdmRate54Mbps)", m_phyModeAarf);
    cmd.AddValue("phyModeFixedLow", "Fixed low WiFi Phy Mode (e.g., OfdmRate6Mbps)", m_phyModeFixedLow);
    cmd.AddValue("phyModeFixedHigh", "Fixed high WiFi Phy Mode (e.g., OfdmRate54Mbps)", m_phyModeFixedHigh);
    cmd.AddValue("txPower", "WiFi Tx Power (dBm)", m_txPower);
    cmd.AddValue("mobilitySpeed", "RandomWalk2dMobilityModel speed (m/s)", m_mobilitySpeed);
    cmd.AddValue("mobilityPause", "RandomWalk2dMobilityModel pause (s)", m_mobilityPause);
    cmd.AddValue("arenaX", "Mobility arena X dimension", m_arenaX);
    cmd.AddValue("arenaY", "Mobility arena Y dimension", m_arenaY);
    cmd.AddValue("pcapTracing", "Enable PCAP tracing", m_pcapTracing);
    cmd.AddValue("flowMonitor", "Enable FlowMonitor", m_flowMonitor);
    cmd.AddValue("netanimTracing", "Enable NetAnim tracing", m_netanimTracing);
    cmd.AddValue("seed", "RNG seed", m_seed);
    cmd.Parse();

    // Sanity checks
    if (m_numNodes < 2) {
        NS_FATAL_ERROR("Number of nodes must be at least 2.");
    }
    if (m_numFlows == 0 && m_numNodes >= 2) {
         m_numFlows = 1; // Ensure at least one flow if nodes allow
         NS_LOG_INFO("Number of flows set to 1 as it was 0, but nodes exist.");
    }
}

void MyWiFiMultiRateExperiment::SetupLogging()
{
    // Enable logging components for debugging and information
    LogComponentEnable("MyWiFiMultiRateExperiment", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_WARN);
    LogComponentEnable("PacketSink", LOG_LEVEL_WARN);
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_WARN); // AODV
    LogComponentEnable("WifiPhy", LOG_LEVEL_INFO); // Useful for checking Tx/Rx
    LogComponentEnable("WifiMac", LOG_LEVEL_INFO);
    LogComponentEnable("ConstantRateWifiManager", LOG_LEVEL_INFO); // Monitor fixed rates
    LogComponentEnable("AarfWifiManager", LOG_LEVEL_INFO); // Monitor AARF adaptation
}

void MyWiFiMultiRateExperiment::SetupNodes()
{
    NS_LOG_INFO("Creating " << m_numNodes << " nodes.");
    m_nodes.Create(m_numNodes);

    // Install Internet Stack with AODV routing
    InternetStackHelper internet;
    AodvHelper aodv;
    internet.SetRoutingHelper(aodv);
    internet.Install(m_nodes);
}

void MyWiFiMultiRateExperiment::SetupWifi()
{
    NS_LOG_INFO("Setting up WiFi with multirate configuration.");

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n); // Using 802.11n for wider rate options

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel",
                               "Frequency", DoubleValue(5.18e9)); // 5.18 GHz

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.SetErrorRateModel("ns3::NistErrorRateModel");
    phy.SetTxPowerStart(m_txPower);
    phy.SetTxPowerEnd(m_txPower);

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    mac.SetType("ns3::AdhocWifiMac");

    // Default configuration for most nodes: AARF rate adaptation
    wifi.SetRemoteStationManager("ns3::AarfWifiManager", "DataMode", StringValue(m_phyModeAarf));
    m_devices = wifi.Install(phy, mac, m_nodes);

    // Apply specific fixed rate configurations for subsets of nodes:
    // Nodes 0 to (m_numNodes/4 - 1) will use a fixed low rate.
    // Nodes (m_numNodes/4) to (m_numNodes/2 - 1) will use a fixed high rate.
    // The rest will use AARF (default).
    uint32_t numFixedLowRateNodes = m_numNodes / 4;
    uint32_t numFixedHighRateNodes = m_numNodes / 4;

    NS_LOG_INFO("Configuring " << numFixedLowRateNodes << " nodes with fixed low rate (" << m_phyModeFixedLow << ").");
    NS_LOG_INFO("Configuring " << numFixedHighRateNodes << " nodes with fixed high rate (" << m_phyModeFixedHigh << ").");

    for (uint32_t i = 0; i < numFixedLowRateNodes && i < m_numNodes; ++i)
    {
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue(m_phyModeFixedLow),
                                     "ControlMode", StringValue(m_phyModeFixedLow));
        Ptr<NetDevice> newDevice = wifi.Install(phy, mac, NodeContainer(m_nodes.Get(i))).Get(0);
        m_devices.Replace(i, newDevice); // Replace the device in the container
        NS_LOG_INFO("Node " << i << " set to fixed low rate: " << m_phyModeFixedLow);
    }

    for (uint32_t i = numFixedLowRateNodes; i < numFixedLowRateNodes + numFixedHighRateNodes && i < m_numNodes; ++i)
    {
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue(m_phyModeFixedHigh),
                                     "ControlMode", StringValue(m_phyModeFixedHigh));
        Ptr<NetDevice> newDevice = wifi.Install(phy, mac, NodeContainer(m_nodes.Get(i))).Get(0);
        m_devices.Replace(i, newDevice); // Replace the device in the container
        NS_LOG_INFO("Node " << i << " set to fixed high rate: " << m_phyModeFixedHigh);
    }

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    m_interfaces = ipv4.Assign(m_devices);

    // Enable PCAP tracing
    if (m_pcapTracing)
    {
        phy.EnablePcapAll("wifi-multirate-pcap");
    }
}

void MyWiFiMultiRateExperiment::SetupMobility()
{
    NS_LOG_INFO("Setting up RandomWalk2dMobilityModel for " << m_numNodes << " nodes.");
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(m_arenaX) + "]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(m_arenaY) + "]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", DoubleValue(m_mobilityPause),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(m_mobilitySpeed) + "]"),
                              "Bounds", StringValue("0|"+std::to_string(m_arenaX)+"|0|"+std::to_string(m_arenaY)));
    mobility.Install(m_nodes);

    if (m_netanimTracing)
    {
        AnimationInterface anim("wifi-multirate-netanim.xml");
        // Optional: Set node positions from MobilityModel for initial state
        for (uint32_t i = 0; i < m_nodes.GetN(); ++i)
        {
            Ptr<Node> node = m_nodes.Get(i);
            Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
            if (mob)
            {
                anim.SetConstantPosition(node, mob->GetPosition().x, mob->GetPosition().y);
            }
        }
    }
}

void MyWiFiMultiRateExperiment::SetupRouting()
{
    // AODV routing helper was already set in SetupNodes() via InternetStackHelper.
    NS_LOG_INFO("AODV routing is configured implicitly via InternetStackHelper.");
}

void MyWiFiMultiRateExperiment::SetupApplications()
{
    NS_LOG_INFO("Setting up " << m_numFlows << " UDP OnOff applications.");

    uint16_t port = 9; // Common port for applications

    Ptr<UniformRandomVariable> rv = CreateObject<UniformRandomVariable>();
    rv->SetAttribute("Min", DoubleValue(0));
    rv->SetAttribute("Max", DoubleValue(m_nodes.GetN() - 1));

    for (uint32_t i = 0; i < m_numFlows; ++i)
    {
        uint32_t srcNodeId = rv->GetInteger();
        uint32_t dstNodeId = rv->GetInteger();

        // Ensure source and destination are different
        while (srcNodeId == dstNodeId)
        {
            dstNodeId = rv->GetInteger();
        }

        Ptr<Node> srcNode = m_nodes.Get(srcNodeId);
        Ptr<Node> dstNode = m_nodes.Get(dstNodeId);

        Ipv4Address dstAddress = m_interfaces.GetAddress(dstNodeId);

        // Packet Sink (Receiver)
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApps = sinkHelper.Install(dstNode);
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(m_simTime));

        // OnOff Application (Sender)
        OnOffHelper onoffHelper("ns3::UdpSocketFactory", InetSocketAddress(dstAddress, port));
        onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
        onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // Always On
        onoffHelper.SetAttribute("DataRate", StringValue("1Mbps")); // Packet size is 1024 bytes, data rate controls sending
        onoffHelper.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes per packet

        ApplicationContainer sourceApps = onoffHelper.Install(srcNode);
        // Stagger start times slightly to avoid initial congestion peak
        sourceApps.Start(Seconds(1.0 + i * 0.01));
        sourceApps.Stop(Seconds(m_simTime - 1.0));

        NS_LOG_INFO("Flow " << i << ": Node " << srcNodeId << " (" << m_interfaces.GetAddress(srcNodeId)
                           << ") -> Node " << dstNodeId << " (" << dstAddress << ")");
    }

    // Register a trace sink for every WifiNetDevice's MacRx trace source
    // This is for aggregate throughput calculation across all devices
    for (uint32_t i = 0; i < m_devices.GetN(); ++i)
    {
        Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice>(m_devices.Get(i));
        // Connect to the MacRx trace source to count total received bytes
        device->GetMac()->TraceConnectWithoutContext("MacRx", MakeCallback(&MyWiFiMultiRateExperiment::CheckRx, this));
    }
}

void MyWiFiMultiRateExperiment::CheckRx(Ptr<const Packet> p, const Address& addr)
{
    // Accumulate total bytes received for aggregate throughput calculation
    m_totalRxBytes += p->GetSize();
}

void MyWiFiMultiRateExperiment::MonitorThroughput()
{
    double currentTime = Simulator::Now().GetSeconds();
    double timeElapsed = currentTime - m_lastThroughputCheckTime;

    if (timeElapsed > 0.0)
    {
        // Calculate throughput for the last interval in Mbps
        double currentThroughputMbps = (m_totalRxBytes * 8.0) / (timeElapsed * 1000000.0);
        NS_LOG_INFO("At " << currentTime << "s, Aggregate Throughput: " << currentThroughputMbps << " Mbps");
        m_throughputDataFile << currentTime << "\t" << currentThroughputMbps << "\n";

        // Reset counters for the next interval
        m_totalRxBytes = 0;
        m_lastThroughputCheckTime = currentTime;
    }

    // Schedule the next throughput check every 1 second
    Simulator::Schedule(Seconds(1.0), &MyWiFiMultiRateExperiment::MonitorThroughput, this);
}

void MyWiFiMultiRateExperiment::ReportResults()
{
    NS_LOG_INFO("Generating simulation results.");

    // Close the throughput data file
    m_throughputDataFile.close();

    // Generate Gnuplot script and PNG image for aggregate throughput
    m_plotHelper.Add2dPlot(m_throughputPlot);
    m_plotHelper.GenerateOutput(m_plotFileName, true); // true generates a PNG image

    if (m_flowMonitor)
    {
        // Collect and serialize Flow Monitor statistics to XML file
        m_flowMonitorPtr->CheckForStats();
        m_flowMonitorHelper.SerializeToXmlFile("wifi-multirate-flowmon.xml", true, true);

        // Print Flow Monitor statistics to console
        std::map<FlowId, FlowMonitor::FlowStats> stats = m_flowMonitorPtr->Get:</FlowId, FlowMonitor::FlowStats>();
        NS_LOG_INFO("--- Flow Monitor Statistics ---");
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
        {
            Ipv4FlowClassifier::FiveTuple t = m_flowMonitorHelper.GetClassifier()->FindFlow(i->first);
            std::cout << "Flow " << i->first << " (" << t.sourceAddress << ":" << t.sourcePort
                      << " -> " << t.destinationAddress << ":" << t.destinationPort << ")\n";
            std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
            std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
            std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
            std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
            if (i->second.rxPackets > 0)
            {
                std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000.0 << " Mbps\n";
                std::cout << "  Delay:      " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";
                std::cout << "  Jitter:     " << i->second.jitterSum.GetSeconds() / (i->second.rxPackets > 1 ? (i->second.rxPackets - 1) : 1) << " s\n";
            }
            std::cout << "  Loss Ratio: " << (double)(i->second.txPackets - i->second.rxPackets) / i->second.txPackets << "\n";
            std::cout << "\n";
        }
    }
}

void MyWiFiMultiRateExperiment::Run()
{
    NS_LOG_INFO("Starting WiFi Multi-rate Experiment simulation.");

    SetupNodes();
    SetupWifi();
    SetupMobility();
    SetupApplications();

    // Schedule periodic throughput monitoring from the start
    Simulator::Schedule(Seconds(0.0), &MyWiFiMultiRateExperiment::MonitorThroughput, this);
    m_lastThroughputCheckTime = 0.0; // Initialize for the first interval

    // Flow Monitor setup (installs classifiers and probes on devices)
    if (m_flowMonitor)
    {
        m_flowMonitorPtr = m_flowMonitorHelper.InstallAll();
    }

    // Stop simulation after m_simTime seconds
    Simulator::Stop(Seconds(m_simTime));
    Simulator::Run();

    // Report results after simulation finishes
    ReportResults();

    Simulator::Destroy();
    NS_LOG_INFO("Simulation finished.");
}

} // namespace ns3

int main(int argc, char *argv[])
{
    // Create an instance of the experiment class and run the simulation
    ns3::MyWiFiMultiRateExperiment experiment;
    experiment.Run();

    return 0;
}