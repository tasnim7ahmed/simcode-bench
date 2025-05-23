#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/udp-client-helper.h"

int main(int argc, char *argv[]) {
    // 1. Configure logging for relevant components (optional, but good for debugging)
    ns3::LogComponentEnable("WifiHelper", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpClient", ns3::LOG_LEVEL_INFO);

    // 2. Create two nodes: one for the Access Point (AP) and one for the Station (STA)
    ns3::NodeContainer nodes;
    nodes.Create(2); // Node 0: AP, Node 1: STA

    ns3::Ptr<ns3::Node> apNode = nodes.Get(0);
    ns3::Ptr<ns3::Node> staNode = nodes.Get(1);

    // 3. Configure Wi-Fi for 802.11n standard
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n);

    // Configure Wi-Fi MAC layer for AP and STA
    ns3::WifiMacHelper apMac;
    ns3::WifiMacHelper staMac;
    ns3::Ssid ssid = ns3::Ssid("my-wifi-network");

    // AP MAC configuration
    apMac.SetType("ns3::ApWifiMac",
                  "Ssid", ns3::SsidValue(ssid),
                  "BeaconInterval", ns3::TimeValue(ns3::Seconds(1.0)));
    
    // STA MAC configuration
    staMac.SetType("ns3::StaWifiMac",
                   "Ssid", ns3::SsidValue(ssid),
                   "ActiveProbing", ns3::BooleanValue(false)); // STA doesn't need to probe explicitly

    // Configure Wi-Fi PHY layer (physical transmission)
    ns3::HtWifiPhyHelper wifiPhy; // Using HtWifiPhyHelper for 802.11n support
    wifiPhy.SetChannel(ns3::YansWifiChannelHelper::Create()); // Create a YansWifiChannel

    // Install Wi-Fi devices on nodes
    ns3::NetDeviceContainer apDevices = wifi.Install(wifiPhy, apMac, apNode);
    ns3::NetDeviceContainer staDevices = wifi.Install(wifiPhy, staMac, staNode);

    // 4. Configure Mobility: Both AP and STA are stationary
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    
    // Set specific positions for AP and STA
    ns3::Ptr<ns3::ConstantPositionMobilityModel> apMobility = apNode->GetObject<ns3::ConstantPositionMobilityModel>();
    apMobility->SetPosition(ns3::Vector(0.0, 0.0, 0.0)); // AP at origin

    ns3::Ptr<ns3::ConstantPositionMobilityModel> staMobility = staNode->GetObject<ns3::ConstantPositionMobilityModel>();
    staMobility->SetPosition(ns3::Vector(1.0, 0.0, 0.0)); // STA 1 meter away from AP

    // 5. Install Internet Stack (TCP/IP) on both nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // 6. Assign IP Addresses to the Wi-Fi devices on the 192.168.1.0/24 subnet
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apDevices); // AP gets 192.168.1.1
    ns3::Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices); // STA gets 192.168.1.2

    ns3::Ipv4Address apIpAddress = apInterfaces.GetAddress(0); // Get AP's assigned IP address

    // 7. Configure UDP Applications
    uint16_t port = 9; // UDP port for communication

    // Server (PacketSink) on the AP
    ns3::PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                    ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer serverApps = sinkHelper.Install(apNode);
    serverApps.Start(ns3::Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(ns3::Seconds(13.0)); // Server runs until the end of the simulation

    // Client (UdpClient) on the STA
    ns3::UdpClientHelper clientHelper(apIpAddress, port);
    clientHelper.SetAttribute("MaxPackets", ns3::UintegerValue(100)); // Send 100 packets
    clientHelper.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(0.1))); // 0.1-second intervals
    clientHelper.SetAttribute("PacketSize", ns3::UintegerValue(512)); // 512 bytes per packet

    ns3::ApplicationContainer clientApps = clientHelper.Install(staNode);
    clientApps.Start(ns3::Seconds(2.0)); // Client starts at 2 seconds
    // Client will send 100 packets over 100 * 0.1s = 10 seconds.
    // Starting at 2s, it will finish sending around 2s + 10s = 12s.
    clientApps.Stop(ns3::Seconds(12.1)); // Stop client app slightly after all packets are sent

    // 8. Run the simulation
    ns3::Simulator::Stop(ns3::Seconds(13.0)); // Total simulation duration
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}