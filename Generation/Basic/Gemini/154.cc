#include <fstream>
#include <map>
#include <string>
#include <iomanip>
#include <vector>
#include <algorithm> // For std::sort

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

// Use ns3 namespace directly to avoid verbose ns3::
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessPacketDatasetGenerator");

// Structure to hold packet data
struct PacketRecord {
    uint32_t srcNodeId;
    uint32_t dstNodeId;
    uint32_t packetSize;
    Time txTime;
    Time rxTime;

    // Default constructor to initialize sentinel values
    PacketRecord() : srcNodeId(0xFFFFFFFF), dstNodeId(0xFFFFFFFF), packetSize(0), txTime(Time(-1)), rxTime(Time(-1)) {}

    // Comparator for sorting by transmission time
    bool operator<(const PacketRecord& other) const {
        return txTime < other.txTime;
    }
};

// Global map to store packet records, indexed by packet Uid
std::map<uint64_t, PacketRecord> g_packetRecords;

// Output file stream for CSV
std::ofstream g_outputCsvFile;

// Helper to extract node ID from context string
uint32_t GetNodeIdFromContext(std::string context) {
    size_t nodePos = context.find("/NodeList/");
    if (nodePos == std::string::npos) {
        return 0xFFFFFFFF; // Invalid ID
    }
    std::string s = context.substr(nodePos + std::string("/NodeList/").length());
    return std::stoul(s.substr(0, s.find("/")));
}

// Callback for PhyTxEnd trace
// This trace point is fired when the physical layer finishes transmitting a packet on the air.
void PhyTxEndTrace(std::string context, Ptr<const Packet> packet) {
    uint64_t packetUid = packet->GetUid();
    uint32_t sourceNodeId = GetNodeIdFromContext(context);

    // If packet not yet in map, create a new record
    if (g_packetRecords.find(packetUid) == g_packetRecords.end()) {
        g_packetRecords[packetUid] = PacketRecord();
    }
    
    // Update sender specific info
    g_packetRecords[packetUid].srcNodeId = sourceNodeId;
    g_packetRecords[packetUid].packetSize = packet->GetSize();
    g_packetRecords[packetUid].txTime = Simulator::Now();
    NS_LOG_DEBUG("Tx: Packet UID " << packetUid << " from Node " << sourceNodeId << " at " << Simulator::Now().GetSeconds() << "s");
}

// Callback for PhyRxEnd trace
// This trace point is fired when the physical layer finishes receiving a packet from the air.
void PhyRxEndTrace(std::string context, Ptr<const Packet> packet) {
    uint64_t packetUid = packet->GetUid();
    uint32_t destNodeId = GetNodeIdFromContext(context);

    // If packet not yet in map, create a new record
    if (g_packetRecords.find(packetUid) == g_packetRecords.end()) {
        g_packetRecords[packetUid] = PacketRecord();
    }
    
    // Update receiver specific info
    g_packetRecords[packetUid].dstNodeId = destNodeId;
    g_packetRecords[packetUid].rxTime = Simulator::Now();
    NS_LOG_DEBUG("Rx: Packet UID " << packetUid << " at Node " << destNodeId << " at " << Simulator::Now().GetSeconds() << "s");
}

int main(int argc, char *argv[]) {
    // Optional: Enable logging for debugging
    // LogComponentEnable("WirelessPacketDatasetGenerator", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("WifiPhy", LOG_LEVEL_DEBUG); 
    // LogComponentEnable("YansWifiPhy", LOG_LEVEL_DEBUG);

    // Command line arguments
    double simTime = 15.0; // Total simulation time in seconds
    uint32_t numNodes = 5;
    uint32_t centralServerNodeId = 4; // Node ID of the central server (0-indexed)
    uint32_t packetSize = 1024;       // Size of UDP packets in bytes
    double packetInterval = 0.1;      // Interval between packet transmissions for clients (seconds)
    uint32_t maxPackets = 100;        // Maximum packets to send per client

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total simulation time in seconds", simTime);
    cmd.AddValue("numNodes", "Number of nodes in the network", numNodes);
    cmd.AddValue("centralServerNodeId", "ID of the central server node (0-indexed)", centralServerNodeId);
    cmd.AddValue("packetSize", "Size of UDP packets in bytes", packetSize);
    cmd.AddValue("packetInterval", "Interval between packet transmissions for clients", packetInterval);
    cmd.AddValue("maxPackets", "Maximum packets to send per client", maxPackets);
    cmd.Parse(argc, argv);

    if (numNodes < 2 || centralServerNodeId >= numNodes) {
        NS_FATAL_ERROR("Invalid number of nodes or central server ID. Central server ID must be < numNodes.");
    }

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // 2. Install mobility model (ConstantPositionMobilityModel)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    
    // Node 0 (client) at (-20, 0, 0)
    // Node 1 (client) at (20, 0, 0)
    // Node 2 (client) at (0, -20, 0)
    // Node 3 (client) at (0, 20, 0)
    // Node 4 (server) at (0, 0, 0)
    // These positions are assigned sequentially to nodes created in the NodeContainer (Node 0, Node 1, ..., Node 4)
    positionAlloc->Add(Vector(-20.0, 0.0, 0.0)); // Node 0
    positionAlloc->Add(Vector(20.0, 0.0, 0.0));  // Node 1
    positionAlloc->Add(Vector(0.0, -20.0, 0.0)); // Node 2
    positionAlloc->Add(Vector(0.0, 20.0, 0.0));  // Node 3
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // Node 4 (Server)
    
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // 3. Configure and install Wifi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n); // Using 802.11n standard

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO); 
    // Set fixed TxPower to ensure consistent signal strength
    wifiPhy.Set("TxPowerStart", DoubleValue(16.0)); // 16 dBm = ~40 mW
    wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));
    wifiPhy.Set("TxPowerLevels", UintegerValue(1));
    wifiPhy.Set("RxGain", DoubleValue(0.0));        // dB
    wifiPhy.Set("TxGain", DoubleValue(0.0));        // dB
    // Energy Detection Threshold and CCA Threshold affect carrier sensing
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-85.0)); // dBm
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-88.0));      // dBm

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    // Using FriisPropagationLossModel with a typical 5 GHz frequency
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel",
                                   "Frequency", DoubleValue(5.180e9)); // 5.180 GHz (channel 36)

    Ptr<YansWifiChannel> channel = wifiChannel.Create();
    wifiPhy.SetChannel(channel);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc mode for a fully connected mesh-like network

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // 4. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 6. Set up UDP applications
    uint16_t port = 9; // Standard echo port

    // Server application on the central server node
    UdpServerHelper serverHelper(port);
    ApplicationContainer serverApp = serverHelper.Install(nodes.Get(centralServerNodeId));
    serverApp.Start(Seconds(0.0));          // Start server at the beginning of simulation
    serverApp.Stop(Seconds(simTime + 1.0)); // Stop server after simulation duration

    // Client applications on all other nodes
    for (uint32_t i = 0; i < numNodes; ++i) {
        if (i == centralServerNodeId) {
            continue; // Skip the server node itself
        }

        // Get the IP address of the server node
        Ipv4Address serverAddress = interfaces.GetAddress(centralServerNodeId);

        UdpClientHelper clientHelper(serverAddress, port);
        clientHelper.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        clientHelper.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
        clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        
        ApplicationContainer clientApp = clientHelper.Install(nodes.Get(i));
        clientApp.Start(Seconds(1.0)); // Start clients after 1 second to allow network to settle
        clientApp.Stop(Seconds(simTime)); // Stop clients at the end of simulation duration
    }

    // 7. Data Collection setup
    // Connect trace sinks for PhyTxEnd and PhyRxEnd on all WifiPhy devices
    // The path uses '*' as a wildcard to connect to all instances of WifiPhy in any node/device
    Config::Connect("/NodeList/*/DeviceList/*/Phy/PhyTxEnd", MakeCallback(&PhyTxEndTrace));
    Config::Connect("/NodeList/*/DeviceList/*/Phy/PhyRxEnd", MakeCallback(&PhyRxEndTrace));

    // Open CSV file for writing and write header
    g_outputCsvFile.open("packet_transmissions.csv");
    g_outputCsvFile << "Source Node,Destination Node,Packet Size (bytes),Transmission Time (s),Reception Time (s)\n";

    // 8. Run simulation
    Simulator::Stop(Seconds(simTime + 2.0)); // Stop simulation a bit after applications finish
    Simulator::Run();

    // 9. Process and write collected data to CSV
    NS_LOG_INFO("Writing collected packet data to packet_transmissions.csv...");
    
    // Copy records to a vector for sorting by transmission time
    std::vector<PacketRecord> sortedRecords;
    for (const auto& pair : g_packetRecords) {
        const PacketRecord& record = pair.second;
        // Only include packets that have both Tx and Rx times recorded and valid node IDs
        if (record.txTime != Time(-1) && record.rxTime != Time(-1) && 
            record.srcNodeId != 0xFFFFFFFF && record.dstNodeId != 0xFFFFFFFF) {
            sortedRecords.push_back(record);
        }
    }

    std::sort(sortedRecords.begin(), sortedRecords.end()); // Sort by transmission time

    for (const auto& record : sortedRecords) {
        g_outputCsvFile << record.srcNodeId << ","
                        << record.dstNodeId << ","
                        << record.packetSize << ","
                        << std::fixed << std::setprecision(9) << record.txTime.GetSeconds() << ","
                        << std::fixed << std::setprecision(9) << record.rxTime.GetSeconds() << "\n";
    }
    g_outputCsvFile.close();

    Simulator::Destroy();
    NS_LOG_INFO("Simulation finished.");

    return 0;
}