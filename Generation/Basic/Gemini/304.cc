#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

int main(int argc, char *argv[])
{
    // 1. Enable logging for relevant components
    ns3::LogComponentEnable("WifiHelper", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpClient", ns3::LOG_LEVEL_INFO);

    // Simulation duration
    double simulationTime = 10.0; // seconds

    // 2. Create nodes: 1 AP, 2 STAs
    ns3::NodeContainer apNode;
    apNode.Create(1);
    ns3::NodeContainer staNodes;
    staNodes.Create(2); // staNodes.Get(0) is STA0, staNodes.Get(1) is STA1

    // 3. Configure mobility: Place nodes at fixed positions
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    
    mobility.Install(apNode);
    apNode.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0)); // AP at origin

    mobility.Install(staNodes);
    staNodes.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(10.0, 0.0, 0.0)); // STA0 at (10,0,0)
    staNodes.Get(1)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 10.0, 0.0)); // STA1 at (0,10,0)

    // 4. Configure Wi-Fi
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211g); // Set 802.11g standard

    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    ns3::WifiMacHelper mac;
    ns3::Ssid ssid = ns3::Ssid("ns3-wifi-network");

    // Configure AP MAC
    mac.SetType("ns3::ApWifiMac",
                "Ssid", ns3::SsidValue(ssid),
                "BeaconInterval", ns3::TimeValue(ns3::MicroSeconds(102400))); // Default beacon interval

    ns3::NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    // Configure STA MACs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", ns3::SsidValue(ssid),
                "ActiveProbing", ns3::BooleanValue(false)); // Disable active probing

    ns3::NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // 5. Install Internet Stack
    ns3::InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    // 6. Assign IP Addresses (192.168.1.x subnet)
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");

    ns3::Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apDevices);
    ns3::Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);

    // 7. Set up UDP Applications
    uint16_t serverPort = 8080; // UDP server port

    // Server: Station 1 (staNodes.Get(1)) acts as a UDP server listening on port 8080
    ns3::PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                           ns3::InetSocketAddress(ns3::Ipv4Address::Any(), serverPort));
    ns3::ApplicationContainer serverApps = packetSinkHelper.Install(staNodes.Get(1));
    serverApps.Start(ns3::Seconds(1.0)); // Start server slightly before client
    serverApps.Stop(ns3::Seconds(simulationTime));

    // Client: Station 0 (staNodes.Get(0)) acts as a UDP client
    // Sends to Station 1's IP address
    ns3::Ipv4Address serverAddress = staInterfaces.GetAddress(1); // IP address of STA1
    uint32_t payloadSize = 1024; // 1024-byte packets

    ns3::UdpClientHelper udpClient(serverAddress, serverPort);
    udpClient.SetAttribute("MaxPackets", ns3::UintegerValue(1000)); // Send a maximum of 1000 packets
    udpClient.SetAttribute("Interval", ns3::TimeValue(ns3::MilliSeconds(10))); // Send every 10 milliseconds
    udpClient.SetAttribute("PacketSize", ns3::UintegerValue(payloadSize));

    ns3::ApplicationContainer clientApps = udpClient.Install(staNodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0)); // Start sending at 2 seconds
    clientApps.Stop(ns3::Seconds(simulationTime));

    // 8. Configure Flow Monitor (for analysis and verification)
    ns3::FlowMonitorHelper flowmon;
    ns3::Ptr<ns3::FlowMonitor> monitor = flowmon.InstallAll();

    // 9. Run Simulation
    ns3::Simulator::Stop(ns3::Seconds(simulationTime));
    ns3::Simulator::Run();

    // 10. Process Flow Monitor results and write to XML file
    monitor->CheckForIpv4Flows(); // Ensure all IPv4 flows are registered
    monitor->SerializeToXmlFile("wifi-udp-simulation.xml", true, true);

    ns3::Simulator::Destroy();

    return 0;
}