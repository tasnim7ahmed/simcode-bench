#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    // 1. Simulation parameters
    double simulationTime = 10.0; // seconds
    uint32_t numSta = 3;
    uint32_t udpPort = 9; // UDP server port

    // 2. Create nodes
    ns3::NodeContainer apNode;
    apNode.Create(1);
    ns3::NodeContainer staNodes;
    staNodes.Create(numSta);

    // 3. Configure Mobility
    // AP: Stationary
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    // STAs: Random Walk 2D mobility
    // Set a random initial position within a square of 100x100m for STAs
    double minX = 0.0, maxX = 100.0;
    double minY = 0.0, maxY = 100.0;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", ns3::StringValue("ns3::UniformRandomVariable[Min=" + std::to_string(minX) + "|Max=" + std::to_string(maxX) + "]"),
                                  "Y", ns3::StringValue("ns3::UniformRandomVariable[Min=" + std::to_string(minY) + "|Max=" + std::to_string(maxY) + "]"));
    // Set RandomWalk2dMobilityModel with bounds and constant speed
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", ns3::RectangleValue(ns3::Rectangle(minX, maxX, minY, maxY)),
                              "Speed", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1.0]")); // 1 m/s
    mobility.Install(staNodes);

    // 4. Configure Wi-Fi
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211n_5GHZ); // Example: 802.11n at 5GHz

    // Set AarfWifiManager for rate control
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    // Wi-Fi Channel configuration
    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    // Using a frequency typical for 802.11n 5GHz band
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", ns3::DoubleValue(5.18e9));

    // Wi-Fi Phy configuration
    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Wi-Fi MAC configuration for AP
    ns3::NqosWifiMacHelper apMac = ns3::NqosWifiMacHelper::Default();
    ns3::Ssid ssid = ns3::Ssid("my-wifi-network");
    apMac.SetType("ns3::ApWifiMac", "Ssid", ns3::SsidValue(ssid));
    ns3::NetDeviceContainer apDevices = wifi.Install(phy, apMac, apNode);

    // Wi-Fi MAC configuration for STAs
    ns3::NqosWifiMacHelper staMac = ns3::NqosWifiMacHelper::Default();
    staMac.SetType("ns3::StaWifiMac", "Ssid", ns3::SsidValue(ssid), "ActiveProbing", ns3::BooleanValue(false));
    ns3::NetDeviceContainer staDevices = wifi.Install(phy, staMac, staNodes);

    // 5. Install Internet Stack
    ns3::InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    // 6. Assign IP Addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    ns3::Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // 7. Configure Applications
    // UDP Server on the AP
    ns3::PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                           ns3::Address(ns3::InetSocketAddress(apInterfaces.GetAddress(0), udpPort)));
    ns3::ApplicationContainer serverApp = packetSinkHelper.Install(apNode.Get(0));
    serverApp.Start(ns3::Seconds(0.0));
    serverApp.Stop(ns3::Seconds(simulationTime));

    // UDP Client on one STA (e.g., the first STA: staNodes.Get(0))
    ns3::UdpClientHelper udpClient(apInterfaces.GetAddress(0), udpPort); // Target AP's IP and UDP port
    udpClient.SetAttribute("MaxPackets", ns3::UintegerValue(0xFFFFFFFF)); // Send continuously
    udpClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(0.1))); // Send one packet every 0.1 seconds
    udpClient.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // Packet size of 1024 bytes
    ns3::ApplicationContainer clientApp = udpClient.Install(staNodes.Get(0)); // Install on STA 0
    clientApp.Start(ns3::Seconds(1.0)); // Start sending after 1 second
    clientApp.Stop(ns3::Seconds(simulationTime));

    // Optional: Enable PCAP tracing for devices
    // phy.EnablePcap("wifi-ap", apDevices.Get(0));
    // phy.EnablePcap("wifi-sta0", staDevices.Get(0));

    // 8. Run simulation
    ns3::Simulator::Stop(ns3::Seconds(simulationTime));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}