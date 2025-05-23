#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

// Bring in necessary ns-3 namespace
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetWifiP2P");

int
main(int argc, char* argv[])
{
    // Log messages to the console
    LogComponentEnable("VanetWifiP2P", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("WifiPhy", LOG_LEVEL_INFO);

    // Set default simulation parameters
    double simulationTime = 20.0; // seconds
    uint32_t packetSize = 1024;   // bytes
    double sendInterval = 1.0;    // seconds

    // Command line arguments for customization
    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("packetSize", "Size of UDP packets in bytes", packetSize);
    cmd.AddValue("sendInterval", "Interval between UDP packet sends in seconds", sendInterval);
    cmd.Parse(argc, argv);

    // 1. Create Nodes (Vehicle and RSU)
    NodeContainer rsuNode;
    rsuNode.Create(1);

    NodeContainer vehicleNode;
    vehicleNode.Create(1);

    // 2. Install Mobility Models
    MobilityHelper mobility;

    // RSU is static at (0,0,0)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(rsuNode);
    Ptr<ConstantPositionMobilityModel> rsuMobility = rsuNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
    rsuMobility->SetPosition(Vector(0.0, 0.0, 0.0));

    // Vehicle starts at (100,0,0) and moves along the X-axis at 10 m/s
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicleNode);
    Ptr<ConstantVelocityMobilityModel> vehicleMobility = vehicleNode.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    vehicleMobility->SetPosition(Vector(100.0, 0.0, 0.0));
    vehicleMobility->SetVelocity(Vector(-10.0, 0.0, 0.0)); // Moving towards RSU

    // 3. Install Internet Stack
    InternetStackHelper internet;
    internet.Install(rsuNode);
    internet.Install(vehicleNode);

    // 4. Configure Wi-Fi (802.11p/WAVE)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211p);

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    // Create and configure a YansWifiChannel with propagation loss model
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLossModel("ns3::TwoRayGroundPropagationLossModel");
    wifiChannel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    QosWifiMacHelper wifiMac = QosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc mode for VANET

    NetDeviceContainer rsuDevice;
    rsuDevice = wifi.Install(wifiPhy, wifiMac, rsuNode);

    NetDeviceContainer vehicleDevice;
    vehicleDevice = wifi.Install(wifiPhy, wifiMac, vehicleNode);

    // 5. Assign IP Addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0"); // Network 10.0.0.0/24

    Ipv4InterfaceContainer rsuInterfaces = ipv4.Assign(rsuDevice);
    Ipv4InterfaceContainer vehicleInterfaces = ipv4.Assign(vehicleDevice);

    // 6. Configure UDP Applications
    // RSU as UDP Server
    UdpServerHelper udpServer(9); // Listen on port 9
    ApplicationContainer serverApps = udpServer.Install(rsuNode.Get(0));
    serverApps.Start(Seconds(1.0)); // Start server at 1 second
    serverApps.Stop(Seconds(simulationTime));

    // Vehicle as UDP Client
    // Send to RSU's IP address on port 9
    UdpClientHelper udpClient(rsuInterfaces.GetAddress(0), 9);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100000)); // Large number to ensure sending throughout
    udpClient.SetAttribute("Interval", TimeValue(Seconds(sendInterval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = udpClient.Install(vehicleNode.Get(0));
    clientApps.Start(Seconds(2.0)); // Start client at 2 seconds (after server is up)
    clientApps.Stop(Seconds(simulationTime - 0.5)); // Stop just before simulation ends

    // 7. Enable Tracing (Optional but useful for analysis)
    // PCAP traces for Wi-Fi devices
    wifiPhy.EnablePcap("vanet-rsu", rsuDevice.Get(0));
    wifiPhy.EnablePcap("vanet-vehicle", vehicleDevice.Get(0));

    // ASCII trace for mobility
    AsciiTraceHelper ascii;
    mobility.EnableAsciiAll(ascii.CreateFileStream("vanet-mobility.mob"));

    // Set up reporting of current positions for debugging/visualization
    Simulator::Schedule(Seconds(0.0), []() {
        NS_LOG_INFO("Time: " << Simulator::Now().GetSeconds() << "s, RSU Pos: " << Simulator::Get
        Node(0)->GetObject<ConstantPositionMobilityModel>()->GetPosition());
        NS_LOG_INFO("Time: " << Simulator::Now().GetSeconds() << "s, Vehicle Pos: " << Simulator::Get
        Node(1)->GetObject<ConstantVelocityMobilityModel>()->GetPosition());
    });
    Simulator::Schedule(Seconds(1.0), []() {
        NS_LOG_INFO("Time: " << Simulator::Now().GetSeconds() << "s, RSU Pos: " << Simulator::Get
        Node(0)->GetObject<ConstantPositionMobilityModel>()->GetPosition());
        NS_LOG_INFO("Time: " << Simulator::Now().GetSeconds() << "s, Vehicle Pos: " << Simulator::Get
        Node(1)->GetObject<ConstantVelocityMobilityModel>()->GetPosition());
    });
    Simulator::Schedule(Seconds(simulationTime / 2), []() {
        NS_LOG_INFO("Time: " << Simulator::Now().GetSeconds() << "s, RSU Pos: " << Simulator::Get
        Node(0)->GetObject<ConstantPositionMobilityModel>()->GetPosition());
        NS_LOG_INFO("Time: " << Simulator::Now().GetSeconds() << "s, Vehicle Pos: " << Simulator::Get
        Node(1)->GetObject<ConstantVelocityMobilityModel>()->GetPosition());
    });
    Simulator::Schedule(Seconds(simulationTime - 0.1), []() {
        NS_LOG_INFO("Time: " << Simulator::Now().GetSeconds() << "s, RSU Pos: " << Simulator::Get
        Node(0)->GetObject<ConstantPositionMobilityModel>()->GetPosition());
        NS_LOG_INFO("Time: " << Simulator::Now().GetSeconds() << "s, Vehicle Pos: " << Simulator::Get
        Node(1)->GetObject<ConstantVelocityMobilityModel>()->GetPosition());
    });

    // 8. Run Simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}