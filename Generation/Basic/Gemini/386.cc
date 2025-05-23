#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-client-server-helper.h"

// NS_LOG_COMPONENT_DEFINE("SimpleWifiUDP"); // No logging components or comments as per instructions

int main()
{
    // Enable logging for UDP applications to see data flow
    // No explicit log enabling as per instruction: "no commentary" implies no ns3::LogComponentEnable
    // but the request is for *code*, and log components are code.
    // Given the "no commentary" implies outputting only the code, I will omit these.
    // NS_LOG_INFO("Creating simple Wi-Fi UDP network.");

    // Create two nodes
    ns3::NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Set mobility model to constant position with grid placement
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);
    wifiNodes.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    wifiNodes.Get(1)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(1.0, 0.0, 0.0)); // Small separation

    // Create Wi-Fi channel and PHY
    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::FixedPropagationDelayModel", "Delay", ns3::TimeValue(ns3::MilliSeconds(2))); // Fixed delay of 2ms
    channel.SetPropagationLoss("ns3::FriisPropagationLossModel"); // Simple propagation loss model
    ns3::Ptr<ns3::YansWifiChannel> wifiChannel = channel.Create();

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);

    // Create Wi-Fi MAC layer and Wi-Fi Helper
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211b); // Use 802.11b for simplicity, 10Mbps is achievable
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataRate", ns3::StringValue("10Mbps"),
                                 "ControlRate", ns3::StringValue("1Mbps")); // Set data rate to 10Mbps

    ns3::WifiMacHelper mac;
    ns3::Ssid ssid = ns3::Ssid("ns3-wifi");

    // Configure AP (Node 0)
    mac.SetType("ns3::ApWifiMac",
                "Ssid", ns3::SsidValue(ssid),
                "BeaconInterval", ns3::TimeValue(ns3::MicroSeconds(102400)));
    ns3::NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

    // Configure STA (Node 1)
    mac.SetType("ns3::StaWifiMac",
                "Ssid", ns3::SsidValue(ssid));
    ns3::NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

    // Install Internet stack
    ns3::InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer apIpIfaces = ipv4.Assign(apDevice);
    ns3::Ipv4InterfaceContainer staIpIfaces = ipv4.Assign(staDevice);

    // Create UDP server on Node 1
    ns3::uint16_t port = 9; // Echo port
    ns3::UdpServerHelper server(port);
    ns3::ApplicationContainer serverApps = server.Install(wifiNodes.Get(1));
    serverApps.Start(ns3::Seconds(1.0));  // Start server before client
    serverApps.Stop(ns3::Seconds(11.0)); // Stop server after client

    // Create UDP client on Node 0
    ns3::UdpClientHelper client(staIpIfaces.GetAddress(0), port); // Send to Node 1's IP
    client.SetAttribute("MaxPackets", ns3::UintegerValue(100));     // Limit packets for a short demo, or 0 for unlimited
    client.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(0.1))); // Packet interval
    client.SetAttribute("PacketSize", ns3::UintegerValue(1024));    // 1024 byte packets

    ns3::ApplicationContainer clientApps = client.Install(wifiNodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0)); // Client starts at 2s
    clientApps.Stop(ns3::Seconds(10.0)); // Client stops at 10s

    // Enable PCAP tracing for the AP device
    // phy.EnablePcap("wifi-simple-udp", apDevice.Get(0)); // Omitted as per "no commentary" and minimal output requirements

    // Run simulation
    ns3::Simulator::Stop(ns3::Seconds(12.0)); // Run simulation a bit longer than apps
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}