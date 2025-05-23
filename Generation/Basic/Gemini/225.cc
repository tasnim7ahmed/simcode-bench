#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsdv-helper.h"
#include "ns3/random-variable-stream.h"

int main(int argc, char *argv[])
{
    // Enable logging for relevant modules
    ns3::LogComponentEnable("DsdvRouting", ns3::LOG_LEVEL_DEBUG);
    ns3::LogComponentEnable("UdpClient", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpServer", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("WaveNetDevice", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("WaypointMobilityModel", ns3::LOG_LEVEL_DEBUG);

    // Simulation parameters
    uint32_t nVehicles = 10;
    double simTime = 30.0; // seconds
    uint32_t packetSize = 1000; // bytes
    double packetInterval = 1.0; // seconds
    uint32_t maxPackets = static_cast<uint32_t>(simTime / packetInterval); // Send packets for the whole duration

    // Command line arguments for customization
    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("nVehicles", "Number of vehicles", nVehicles);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("packetSize", "UDP packet size in bytes", packetSize);
    cmd.AddValue("packetInterval", "Time interval between packets in seconds", packetInterval);
    cmd.Parse(argc, argv);

    // Create nodes
    ns3::NodeContainer vehicles;
    vehicles.Create(nVehicles);
    ns3::Ptr<ns3::Node> rsuNode = ns3::CreateObject<ns3::Node>();

    // Combine all nodes for common operations
    ns3::NodeContainer allNodes;
    allNodes.Add(vehicles);
    allNodes.Add(rsuNode);

    // Mobility setup
    // RSU is static
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(rsuNode);
    rsuNode->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(500.0, 0.0, 0.0)); // RSU in the middle of a 1km stretch

    // Vehicles use WaypointMobilityModel
    mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.Install(vehicles);

    // Use a fixed seed for reproducible random speeds
    ns3::RngSeedManager::SetSeed(112233);
    ns3::Ptr<ns3::UniformRandomVariable> speedRv = ns3::CreateObject<ns3::UniformRandomVariable>();
    speedRv->SetAttribute("Min", ns3::DoubleValue(10.0)); // min speed 10 m/s (~36 km/h)
    speedRv->SetAttribute("Max", ns3::DoubleValue(30.0)); // max speed 30 m/s (~108 km/h)

    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        ns3::Ptr<ns3::WaypointMobilityModel> wpm = vehicles.Get(i)->GetObject<ns3::WaypointMobilityModel>();
        double initialX = i * 20.0; // Stagger vehicles initially along X-axis
        double speed = speedRv->GetValue(); // Random speed for each vehicle

        // Waypoint 1: Initial position at time 0.0
        wpm->AddWaypoint(ns3::Waypoint(ns3::Seconds(0.0), ns3::Vector(initialX, 0.0, 0.0)));

        // Waypoint 2: Move along X-axis for the simulation duration
        double finalX = initialX + speed * simTime;
        wpm->AddWaypoint(ns3::Waypoint(ns3::Seconds(simTime), ns3::Vector(finalX, 0.0, 0.0)));
    }

    // Wi-Fi setup (802.11p for VANET)
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", ns3::StringValue("OfdmRate6Mbps"),
                                 "RtsCtsThreshold", ns3::UintegerValue(0)); // Disable RTS/CTS

    ns3::YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(ns3::WifiPhyHelper::DLT_IEEE802_11_RADIOTAP);
    
    // Set typical transmit power and receiver sensitivity for vehicular communication
    wifiPhy.Set("TxPowerStart", ns3::DoubleValue(20.0)); // 20 dBm = 100 mW
    wifiPhy.Set("TxPowerEnd", ns3::DoubleValue(20.0));
    wifiPhy.Set("TxPowerLevels", ns3::UintegerValue(1));
    wifiPhy.Set("RxSensitivity", ns3::DoubleValue(-96.0)); // dBm
    wifiPhy.Set("EnergyDetectionThreshold", ns3::DoubleValue(-96.0 + 3.0)); // dBm

    // Set up a propagation channel model
    ns3::YansWifiChannelHelper wifiChannel;
    // Using a simple RangePropagationLossModel for illustrative purposes
    wifiChannel.SetPropagationLossModel("ns3::RangePropagationLossModel", "MaxRange", ns3::DoubleValue(300.0)); // Max range 300m
    wifiChannel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    ns3::WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // All nodes are part of an ad-hoc network

    ns3::NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);

    // Internet stack and DSDV routing
    ns3::DsdvHelper dsdvRouting;
    ns3::InternetStackHelper internet;
    internet.SetRoutingHelper(dsdvRouting); // Install DSDV on all nodes
    internet.Install(allNodes);

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Get RSU's IP address (RSU is the last node added to allNodes)
    ns3::Ipv4Address rsuAddress = interfaces.GetAddress(nVehicles); 

    // Applications
    // UDP Server on RSU
    uint16_t rsuPort = 9; // Discard port is commonly used for simple server examples
    ns3::UdpServerHelper server(rsuPort);
    ns3::ApplicationContainer serverApps = server.Install(rsuNode);
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(simTime + 1.0)); // Run slightly longer than simulation

    // UDP Clients on vehicles
    ns3::UdpClientHelper client(rsuAddress, rsuPort);
    client.SetAttribute("MaxPackets", ns3::UintegerValue(maxPackets));
    client.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(packetInterval)));
    client.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    
    ns3::ApplicationContainer clientApps = client.Install(vehicles);
    clientApps.Start(ns3::Seconds(1.0)); // Start clients after 1 second to allow routing to stabilize
    clientApps.Stop(ns3::Seconds(simTime));

    // Simulation execution
    ns3::Simulator::Stop(ns3::Seconds(simTime + 2.0)); // Stop slightly after applications
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}