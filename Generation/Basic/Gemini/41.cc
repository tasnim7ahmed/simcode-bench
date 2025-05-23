#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-helper.h"

int main(int argc, char *argv[])
{
    // 1. Set up default values
    int gridWidth = 5;
    int gridHeight = 5;
    double gridDistance = 50.0; // meters between nodes
    std::string phyMode = "DsssRate1Mbps";
    uint32_t packetSize = 1000; // bytes
    uint32_t numPackets = 1;
    bool verbose = false;
    bool enablePcap = false;
    bool enableAscii = false;

    // These will be calculated after gridWidth and gridHeight are set
    uint32_t sourceNode = 0; 
    uint32_t sinkNode = 0;   

    // 2. Command-line options
    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("gridWidth", "Width of the grid (number of nodes)", gridWidth);
    cmd.AddValue("gridHeight", "Height of the grid (number of nodes)", gridHeight);
    cmd.AddValue("gridDistance", "Distance between nodes in the grid (meters)", gridDistance);
    cmd.AddValue("phyMode", "Wifi Physical Layer Mode", phyMode);
    cmd.AddValue("packetSize", "Size of UDP packets (bytes)", packetSize);
    cmd.AddValue("numPackets", "Number of UDP packets to send", numPackets);
    cmd.AddValue("sourceNode", "Index of the source node (0 to N-1)", sourceNode);
    cmd.AddValue("sinkNode", "Index of the sink node (0 to N-1)", sinkNode);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("enableAscii", "Enable ASCII tracing", enableAscii);
    cmd.Parse(argc, argv);

    // After parsing command line, calculate default source/sink nodes
    // The highest-numbered node is (gridWidth * gridHeight - 1).
    // Initialize with actual highest node if not set by CLI.
    if (cmd.Get<uint32_t>("sourceNode") == sourceNode && sourceNode == 0) { 
        sourceNode = (gridWidth * gridHeight) - 1;
    }
    // Sink node default is 0, no need to recalculate unless it's changed by CLI.

    // Validate node indices
    uint32_t totalNodes = gridWidth * gridHeight;
    if (sourceNode >= totalNodes) {
        NS_FATAL_ERROR("Source node index (" << sourceNode << ") is out of bounds for " << totalNodes << " nodes.");
    }
    if (sinkNode >= totalNodes) {
        NS_FATAL_ERROR("Sink node index (" << sinkNode << ") is out of bounds for " << totalNodes << " nodes.");
    }
    if (sourceNode == sinkNode) {
        NS_FATAL_ERROR("Source and sink nodes cannot be the same.");
    }

    // 3. Configure logging
    if (verbose) {
        ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
        ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);
        ns3::LogComponentEnable("WifiPhy", ns3::LOG_LEVEL_INFO);
        ns3::LogComponentEnable("OlsrRoutingProtocol", ns3::LOG_LEVEL_INFO);
        ns3::LogComponentEnable("AdhocWifiMac", ns3::LOG_LEVEL_INFO);
    }

    // 4. Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(totalNodes);

    // 5. Configure Mobility (GridPositionAllocator)
    ns3::MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", ns3::DoubleValue(0.0),
                                  "MinY", ns3::DoubleValue(0.0),
                                  "DeltaX", ns3::DoubleValue(gridDistance),
                                  "DeltaY", ns3::DoubleValue(gridDistance),
                                  "GridWidth", ns3::UintegerValue(gridWidth),
                                  "LayoutType", ns3::StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // 6. Configure Wifi
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", ns3::StringValue(phyMode),
                                 "ControlMode", ns3::StringValue(phyMode));

    ns3::YansWifiPhyHelper wifiPhy;
    // Set up channel and propagation loss model for wireless transmission effects
    ns3::YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLossModel("ns3::LogDistancePropagationLossModel",
                                        "Exponent", ns3::DoubleValue(3.0), // Use exponent 3.0 for a typical indoor/urban loss profile
                                        "ReferenceDistance", ns3::DoubleValue(1.0),
                                        "ReferenceLoss", ns3::DoubleValue(46.677)); // Typical loss at 1m for 2.4GHz
    wifiChannel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.SetPcapDataLinkType(ns3::WifiPhyHelper::DLT_IEEE802_11_RADIO);

    ns3::NqosWifiMacHelper wifiMac = ns3::NqosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac"); // Adhoc mode

    ns3::NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // 7. Install Internet Stack with OLSR
    ns3::OlsrHelper olsr;
    ns3::Ipv4ListRoutingHelper listRouting;
    listRouting.Add(olsr, 10); // Priority 10 for OLSR

    ns3::InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting); // Set the routing helper
    internet.Install(nodes);

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 8. Configure Applications (UDP Echo)
    ns3::UdpEchoServerHelper echoServer(9);
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(sinkNode));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(100.0)); // Run for a longer duration

    ns3::UdpEchoClientHelper echoClient(interfaces.GetAddress(sinkNode), 9);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(sourceNode));
    clientApps.Start(ns3::Seconds(2.0)); // Start after server and routing convergence
    clientApps.Stop(ns3::Seconds(2.0 + numPackets * 1.0 + 0.1)); // Stop after all packets are sent

    // 9. Tracing
    if (enablePcap) {
        wifiPhy.EnablePcap("wifi-grid-adhoc", devices);
    }
    if (enableAscii) {
        std::ofstream ascii;
        ascii.open("wifi-grid-adhoc.tr");
        ns3::WifiHelper::EnableAsciiAll(ascii);
    }

    // 10. Simulation
    double simulationTime = 3.0 + numPackets * 1.0 + 1.0; // Allow time for routing + packets + a bit more
    ns3::Simulator::Stop(ns3::Seconds(simulationTime));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}