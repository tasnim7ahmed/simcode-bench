#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

// NS_LOG_COMPONENT_DEFINE ("WifiRandomWalkUdp"); // Not needed in final output, but good for development

int main (int argc, char *argv[])
{
    // Enable logging (optional, but good for debugging)
    // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("WifiMac", LOG_LEVEL_INFO);
    // LogComponentEnable ("RandomWalk2dMobilityModel", LOG_LEVEL_INFO);

    // Command line arguments (not explicitly requested, but useful)
    // CommandLine cmd (__FILE__);
    // cmd.Parse (argc, argv);

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create (2); // Node 0: STA (sender), Node 1: AP (receiver)

    Ptr<Node> staNode = nodes.Get(0);
    Ptr<Node> apNode = nodes.Get(1);

    // 2. Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211n); // 802.11n standard

    YansWifiChannelHelper channel;
    channel.SetPropagationLoss ("ns3::FriisPropagationLossModel");
    channel.SetMimoModel ("ns3::MimoMimoModel"); // Required for 802.11n

    YansWifiPhyHelper phy;
    phy.SetChannel (channel.Create ());
    phy.Set ("Frequency", UintegerValue (5180)); // A common 5GHz frequency for channel 36
    phy.Set ("Antennas", UintegerValue (2)); // For 802.11n, use at least 2 antennas
    phy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (2));
    phy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (2));
    phy.Set ("TxPowerStart", DoubleValue(15.0)); // Default values for 802.11n
    phy.Set ("TxPowerEnd", DoubleValue(15.0));

    WifiMacHelper mac;
    Ssid ssid = Ssid ("MyWifiNetwork");

    // Configure AP MAC
    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid),
                 "BeaconInterval", TimeValue (MicroSeconds (102400)));
    NetDeviceContainer apDevices = wifi.Install (phy, mac, apNode);

    // Configure STA MAC
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
    NetDeviceContainer staDevices = wifi.Install (phy, mac, staNode);

    // 3. Configure Mobility
    MobilityHelper mobility;

    // AP is stationary at a fixed position
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (50.0),
                                   "MinY", DoubleValue (50.0),
                                   "DeltaX", DoubleValue (0.0),
                                   "DeltaY", DoubleValue (0.0),
                                   "GridWidth", UintegerValue (1),
                                   "LayoutType", StringValue ("ns3::GridPositionAllocator::ROW_FIRST"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (apNode);

    // STA uses RandomWalk2dMobilityModel
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)), // 100x100 m^2 area
                               "Distance", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"), // Move 10m then change direction
                               "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]")); // 2 m/s
    mobility.Install (staNode);

    // 4. Install Internet Stack
    InternetStackHelper stack;
    stack.Install (nodes);

    // 5. Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0"); // 10.1.1.0/24 range
    Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

    // 6. Setup UDP Communication (Sender STA, Receiver AP)
    // Receiver (AP) application
    UdpEchoServerHelper echoServer (9); // Port 9
    ApplicationContainer serverApps = echoServer.Install (apNode);
    serverApps.Start (Seconds (1.0)); // Start server early
    serverApps.Stop (Seconds (20.0)); // Stop server after simulation ends

    // Sender (STA) application
    UdpEchoClientHelper echoClient (apInterfaces.GetAddress (0), 9); // Target AP's IP on port 9
    echoClient.SetAttribute ("MaxPackets", UintegerValue (10));       // 10 packets
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));  // 1 second interval
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));     // 1024 bytes per packet
    ApplicationContainer clientApps = echoClient.Install (staNode);
    clientApps.Start (Seconds (2.0)); // Start client after server is up
    clientApps.Stop (Seconds (15.0)); // Stop client after packets are sent

    // 7. Flow Monitor (optional, but useful for statistics)
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    // 8. Run Simulation
    Simulator::Stop (Seconds (20.0)); // Simulate for 20 seconds
    Simulator::Run ();

    // Flow Monitor statistics
    flowMonitor->CheckFor = FlowMonitor::CHECK_ALL_FLOWS;
    flowMonitor->SerializeToXmlFile("wifi-udp-random-walk-flowmon.xml", true, true);

    Simulator::Destroy ();

    return 0;
}