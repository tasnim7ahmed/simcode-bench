#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"

// Define namespaces for convenience
using namespace ns3;
using namespace ns3::olsr;

// --- Parameters ---
static uint32_t nNodes = 9;            // Number of sensor nodes (excluding sink)
static uint32_t sinkNodeId = 0;        // Node ID for the sink (first node created)
static uint32_t packetSize = 1024;     // UDP packet size in bytes
static double intervalSeconds = 1.0;   // Time between packets in seconds
static double simulationTime = 50.0;   // Total simulation time in seconds
static double initialEnergyJ = 100.0;  // Initial energy for each node in Joules
static double txPowerDbm = 15.0;       // WiFi transmit power in dBm (used for WifiPhy)

// --- Trace callback for remaining energy ---
void RemainingEnergyTrace(double oldValue, double newValue)
{
    // This callback can be used to log energy changes in real-time.
    // For large simulations, logging every change might be too verbose.
    // NS_LOG_INFO("Remaining energy changed from " << oldValue << " J to " << newValue << " J");
}

int main(int argc, char *argv[])
{
    // --- Logging ---
    LogComponentEnable("OlsrHelper", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("WifiRadioEnergyModel", LOG_LEVEL_INFO);
    LogComponentEnable("BasicEnergySource", LOG_LEVEL_INFO);

    // --- Command Line Parser ---
    CommandLine cmd(__FILE__);
    cmd.AddValue("nNodes", "Number of sensor nodes", nNodes);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.Parse(argc, argv);

    // Ensure sinkNodeId is within bounds
    if (sinkNodeId >= nNodes + 1)
    {
        NS_FATAL_ERROR("Sink node ID is out of bounds. Must be less than " << (nNodes + 1));
    }

    // --- Node Creation ---
    NodeContainer allNodes;
    allNodes.Create(nNodes + 1); // nNodes sensor nodes + 1 sink node

    NodeContainer sensorNodes;
    for (uint32_t i = 0; i < nNodes + 1; ++i)
    {
        if (i != sinkNodeId)
        {
            sensorNodes.Add(allNodes.Get(i));
        }
    }
    Ptr<Node> sinkNode = allNodes.Get(sinkNodeId);

    // --- Mobility Model (Static positions in a grid) ---
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),  // Distance between nodes on X-axis
                                  "DeltaY", DoubleValue(50.0),  // Distance between nodes on Y-axis
                                  "GridWidth", UintegerValue(4), // 4 nodes per row
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // --- WiFi Setup ---
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ); // Using 802.11n 5GHz for example

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.Set("TxPowerStart", DoubleValue(txPowerDbm));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txPowerDbm));
    wifiPhy.Set("RxGain", DoubleValue(0)); // No Rx gain for simplicity
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-90.0));
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-87.0));

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel",
                                   "MaxRange", DoubleValue(150.0)); // Range for 50m grid spacing
    Ptr<YansWifiChannel> channel = wifiChannel.Create();
    wifiPhy.SetChannel(channel);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // Adhoc mode for WSN

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);

    // --- OLSR Routing Setup ---
    OlsrHelper olsr;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(olsr, 10); // Priority 10 for OLSR

    InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting);
    internet.Install(allNodes);

    // --- IP Address Assignment ---
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // --- Energy Model Setup ---
    BasicEnergySourceHelper basicSourceHelper;
    basicSourceHelper.Set("InitialEnergyJ", DoubleValue(initialEnergyJ));
    basicSourceHelper.Set("EnergyUpdateInterval", TimeValue(Seconds(1.0))); // Update every second

    EnergySourceContainer sources = basicSourceHelper.Install(allNodes);

    WifiRadioEnergyModelHelper wifiEnergyHelper;
    // Set typical current consumption values for a WiFi radio in a sensor node context
    // These values are illustrative and would ideally come from a specific radio's datasheet.
    // TxCurrentA: Current consumed while transmitting (e.g., 100mA for a moderate WiFi module)
    // RxCurrentA: Current consumed while receiving (e.g., 30mA)
    // IdleCurrentA: Current consumed when WiFi is active but not Tx/Rx (e.g., 1mA)
    // SleepCurrentA/StandbyCurrentA: Very low current when radio is in low power states
    wifiEnergyHelper.Set("TxCurrentA", DoubleValue(0.100)); // Example: 100 mA
    wifiEnergyHelper.Set("RxCurrentA", DoubleValue(0.030)); // Example: 30 mA
    wifiEnergyHelper.Set("IdleCurrentA", DoubleValue(0.001)); // Example: 1 mA
    wifiEnergyHelper.Set("SleepCurrentA", DoubleValue(0.000005)); // Example: 5 uA
    wifiEnergyHelper.Set("StandbyCurrentA", DoubleValue(0.000005)); // Example: 5 uA

    // Install WifiRadioEnergyModel on the WifiNetDevices and associate with energy sources
    DeviceEnergyModelContainer deviceModels = wifiEnergyHelper.Install(devices);

    // Trace remaining energy for all nodes (optional: for detailed analysis)
    for (uint32_t i = 0; i < sources.GetN(); ++i)
    {
        Ptr<EnergySource> es = sources.Get(i);
        es->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergyTrace));
    }

    // --- UDP Echo Applications ---
    // Sink (Server) setup
    UdpEchoServerHelper echoServer(9); // Port 9
    ApplicationContainer serverApps = echoServer.Install(sinkNode);
    serverApps.Start(Seconds(1.0)); // Start server after 1 second
    serverApps.Stop(Seconds(simulationTime));

    // Sensor nodes (Clients) setup
    // Get the IP address of the sink node
    Ipv4Address sinkAddress = interfaces.GetAddress(sinkNodeId);
    UdpEchoClientHelper echoClient(sinkAddress, 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295U)); // Send "infinite" packets
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(intervalSeconds)));

    ApplicationContainer clientApps = echoClient.Install(sensorNodes);
    clientApps.Start(Seconds(2.0)); // Start clients after 2 seconds
    clientApps.Stop(Seconds(simulationTime));

    // --- Simulation Run ---
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Print final energy for each node
    for (uint32_t i = 0; i < allNodes.GetN(); ++i)
    {
        Ptr<EnergySource> es = sources.Get(i);
        Ptr<BasicEnergySource> bes = DynamicCast<BasicEnergySource>(es);
        NS_LOG_INFO("Node " << i << " Final Energy: " << bes->GetRemainingEnergy() << " J");
    }

    Simulator::Destroy();

    return 0;
}