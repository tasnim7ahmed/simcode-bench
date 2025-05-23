#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"

// Define a logging component for this simulation
NS_LOG_COMPONENT_DEFINE("WifiEnergySimulation");

// Global pointers to energy sources for final consumption report
static Ptr<ns3::BasicEnergySource> g_senderEnergySource;
static Ptr<ns3::BasicEnergySource> g_receiverEnergySource;

/**
 * \brief Callback function for the RemainingEnergy trace source.
 * \param context A string context, typically identifying the node.
 * \param oldValue The energy before the change (Joules).
 * \param newValue The energy after the change (Joules).
 */
void RemainingEnergyCallback(std::string context, double oldValue, double newValue) {
    NS_LOG_INFO("Time: " << ns3::Simulator::Now().GetSeconds()
                        << "s, " << context << " Remaining Energy: "
                        << oldValue << " J -> " << newValue << " J");
}

/**
 * \brief Callback function for the TotalEnergyConsumption trace source.
 * \param context A string context, typically identifying the node.
 * \param oldValue The total energy consumed before the change (Joules).
 * \param newValue The total energy consumed after the change (Joules).
 */
void TotalEnergyCallback(std::string context, double oldValue, double newValue) {
    NS_LOG_INFO("Time: " << ns3::Simulator::Now().GetSeconds()
                        << "s, " << context << " Total Energy Consumed: "
                        << oldValue << " J -> " << newValue << " J");
}

int main(int argc, char *argv[]) {
    // 1. Configure command-line arguments
    uint32_t packetSize = 1024;        // Size of packets to send (bytes)
    double simulationStartSeconds = 1.0; // Time when applications start (seconds)
    double nodeDistanceMeters = 10.0;  // Distance between the two nodes (meters)
    double simulationDuration = 10.0;  // Total simulation duration (seconds)

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("packetSize", "Size of packets to send (bytes)", packetSize);
    cmd.AddValue("simulationStartSeconds", "Time when applications start (seconds)", simulationStartSeconds);
    cmd.AddValue("nodeDistanceMeters", "Distance between the two nodes (meters)", nodeDistanceMeters);
    cmd.AddValue("simulationDuration", "Total simulation duration (seconds)", simulationDuration);
    cmd.Parse(argc, argv);

    // 2. Enable logging for relevant modules
    ns3::LogComponentEnable("WifiEnergySimulation", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("BasicEnergySource", ns3::LOG_LEVEL_DEBUG);
    ns3::LogComponentEnable("WifiRadioEnergyModel", ns3::LOG_LEVEL_DEBUG);
    // ns3::LogComponentEnable("YansWifiPhy", ns3::LOG_LEVEL_INFO); // Uncomment for more detailed PHY layer logging

    // 3. Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 4. Set up Mobility Model
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    nodes.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(nodeDistanceMeters, 0.0, 0.0));

    // 5. Configure Wi-Fi
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n); // Use 802.11n standard

    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel"); // Simple propagation loss
    ns3::Ptr<ns3::YansWifiChannel> wifiChannel = channel.Create();

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);
    // Set fixed transmit power for the PHY layer (in dBm)
    phy.Set("TxPowerStart", ns3::DoubleValue(20)); // 20 dBm = 100 mW
    phy.Set("TxPowerEnd", ns3::DoubleValue(20));

    ns3::WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac"); // Ad-hoc mode for simplicity

    ns3::NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // 6. Install Internet Stack and assign IP Addresses
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 7. Energy Model Attachment
    double initialEnergyJ = 100.0; // Initial energy for each node (Joules)
    double txPowerWatts = 0.8;    // Power drawn from battery when transmitting (Watts)
    double rxPowerWatts = 0.4;    // Power drawn from battery when receiving (Watts)
    double idlePowerWatts = 0.07; // Power drawn when Wi-Fi radio is idle (Watts)

    // Create and install BasicEnergySource for each node
    ns3::EnergySourceContainer sources;
    ns3::BasicEnergySourceHelper basicEnergySourceHelper;
    basicEnergySourceHelper.Set("InitialEnergyJ", ns3::DoubleValue(initialEnergyJ));
    sources.Install(nodes); // Installs one BasicEnergySource per node

    // Get pointers to the BasicEnergySources for later access (tracing, final report)
    g_senderEnergySource = ns3::DynamicCast<ns3::BasicEnergySource>(sources.Get(0));
    g_receiverEnergySource = ns3::DynamicCast<ns3::BasicEnergySource>(sources.Get(1));

    // Create and install WifiRadioEnergyModel for each Wi-Fi device
    ns3::WifiRadioEnergyModelHelper wifiRadioEnergyModelHelper;
    wifiRadioEnergyModelHelper.Set("TxPower", ns3::DoubleValue(txPowerWatts));
    wifiRadioEnergyModelHelper.Set("RxPower", ns3::DoubleValue(rxPowerWatts));
    wifiRadioEnergyModelHelper.Set("IdlePower", ns3::DoubleValue(idlePowerWatts));
    wifiRadioEnergyModelHelper.Set("SwitchingEnergyJ", ns3::DoubleValue(0.0001)); // Small energy for state transitions

    // Install WifiRadioEnergyModel on the Wi-Fi devices, linking them to their respective energy sources
    ns3::WifiRadioEnergyModelContainer radioEnergyModels = wifiRadioEnergyModelHelper.Install(devices, sources);

    // 8. Energy Tracing
    // Connect RemainingEnergy and TotalEnergyConsumption traces for Sender (Node 0)
    g_senderEnergySource->TraceConnect("RemainingEnergy",
                                       "ns3::WifiEnergySimulation/Sender",
                                       ns3::MakeCallback(&RemainingEnergyCallback));
    g_senderEnergySource->TraceConnect("TotalEnergyConsumption",
                                       "ns3::WifiEnergySimulation/Sender",
                                       ns3::MakeCallback(&TotalEnergyCallback));

    // Connect RemainingEnergy and TotalEnergyConsumption traces for Receiver (Node 1)
    g_receiverEnergySource->TraceConnect("RemainingEnergy",
                                         "ns3::WifiEnergySimulation/Receiver",
                                         ns3::MakeCallback(&RemainingEnergyCallback));
    g_receiverEnergySource->TraceConnect("TotalEnergyConsumption",
                                         "ns3::WifiEnergySimulation/Receiver",
                                         ns3::MakeCallback(&TotalEnergyCallback));

    // 9. Applications (UDP Echo Client/Server)
    // Install UDP Echo Server on Node 1 (receiver)
    uint16_t port = 9;
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(ns3::Seconds(0.0)); // Server starts immediately
    serverApps.Stop(ns3::Seconds(simulationDuration + 1.0)); // Server stops after simulation end

    // Install UDP Echo Client on Node 0 (sender)
    ns3::UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port); // Send to receiver's IP
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(100));     // Send up to 100 packets
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // Send one packet per second
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(simulationStartSeconds));
    clientApps.Stop(ns3::Seconds(simulationDuration)); // Client stops at simulation duration

    // 10. Simulation Execution
    // Set simulator stop time, giving a small buffer after applications stop
    ns3::Simulator::Stop(ns3::Seconds(simulationDuration + 2.0));
    ns3::Simulator::Run();

    // 11. Final Energy Consumption Report
    NS_LOG_INFO("--- Simulation Finished ---");
    NS_LOG_INFO("Sender (Node 0) Final Remaining Energy: "
                << g_senderEnergySource->GetRemainingEnergy() << " J");
    NS_LOG_INFO("Sender (Node 0) Total Energy Consumed: "
                << g_senderEnergySource->GetTotalEnergyConsumption() << " J");
    NS_LOG_INFO("Receiver (Node 1) Final Remaining Energy: "
                << g_receiverEnergySource->GetRemainingEnergy() << " J");
    NS_LOG_INFO("Receiver (Node 1) Total Energy Consumed: "
                << g_receiverEnergySource->GetTotalEnergyConsumption() << " J");

    // Check if energy consumption remained within specified limits (i.e., nodes didn't run out)
    if (g_senderEnergySource->GetRemainingEnergy() > 0 && g_receiverEnergySource->GetRemainingEnergy() > 0) {
        NS_LOG_INFO("Energy consumption remained within limits: Both nodes still have remaining energy.");
    } else {
        NS_LOG_WARN("Energy consumption exceeded limits: One or more nodes ran out of energy!");
    }

    ns3::Simulator::Destroy();
    return 0;
}