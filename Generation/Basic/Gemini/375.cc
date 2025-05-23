#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[]) {
    // 1. Enable logging for relevant modules (optional, but useful for debugging)
    // ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    // ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);
    // ns3::LogComponentEnable("WifiPhy", ns3::LOG_LEVEL_INFO);
    // ns3::LogComponentEnable("RandomWalk2dMobilityModel", ns3::LOG_LEVEL_INFO);

    // 2. Create Nodes
    ns3::NodeContainer apNode;
    apNode.Create(1);
    ns3::NodeContainer staNode;
    staNode.Create(1);

    // 3. Configure Mobility
    ns3::MobilityHelper mobility;

    // AP (Node 1) is stationary at (0,0,0)
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", ns3::DoubleValue(0.0),
                                  "MinY", ns3::DoubleValue(0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    // STA (Node 2) moves randomly within a 100x100 meter area
    // Initial position of STA can be randomized within a disc around the AP.
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", ns3::DoubleValue(0.0),
                                  "Y", ns3::DoubleValue(0.0),
                                  "Rho", ns3::StringValue("ns3::UniformRandomVariable[Min=0|Max=20]")); // Initial placement within 20m of AP

    // RandomWalk2dMobilityModel for STA
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", ns3::RectangleValue(ns3::Rectangle(-50, 50, -50, 50)), // Movement bounds
                              "Speed", ns3::StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"), // Speed between 1 and 5 m/s
                              "Pause", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0.5]")); // Pause for 0.5s after each move
    mobility.Install(staNode);

    // 4. Configure Wi-Fi Devices
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211n_5GHZ); // Using 802.11n 5GHz for modern rates

    // Set up Wifi channel
    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationLossModel("ns3::LogDistancePropagationLossModel", "Exponent", ns3::DoubleValue(3.0));
    channel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    ns3::Ptr<ns3::YansWifiChannel> wifiChannel = channel.Create();

    // Set up Wifi physical layer
    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);

    // Set up Wifi MAC layer (AP and STA specific)
    ns3::WifiMacHelper mac;
    ns3::Ssid ssid = ns3::Ssid("MyWifiNetwork");

    // Configure AP MAC
    mac.SetType("ns3::ApWifiMac",
                "Ssid", ns3::SsidValue(ssid),
                "BeaconInterval", ns3::TimeValue(ns3::Seconds(1)));
    ns3::NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    // Configure STA MAC
    mac.SetType("ns3::StaWifiMac",
                "Ssid", ns3::SsidValue(ssid),
                "ActiveProbing", ns3::BooleanValue(false)); // STA passively listens for beacons
    ns3::NetDeviceContainer staDevices = wifi.Install(phy, mac, staNode);

    // 5. Install Internet Stack
    ns3::InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNode);

    // 6. Assign IP Addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    ns3::Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // 7. Configure UDP Applications
    // Server (Node 1 - AP)
    ns3::UdpEchoServerHelper echoServer(9); // Listen on port 9
    ns3::ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
    serverApps.Start(ns3::Seconds(1.0)); // Start server at 1 second
    serverApps.Stop(ns3::Seconds(9.0));  // Stop server at 9 seconds

    // Client (Node 2 - STA)
    ns3::UdpEchoClientHelper echoClient(apInterfaces.GetAddress(0), 9); // Send to AP's IP on port 9
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(100)); // Max 100 packets to send
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(0.5))); // Send one packet every 0.5 seconds
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // Packet size of 1024 bytes
    ns3::ApplicationContainer clientApps = echoClient.Install(staNode.Get(0));
    clientApps.Start(ns3::Seconds(2.0)); // Start client at 2 seconds (after server is up)
    clientApps.Stop(ns3::Seconds(9.0));  // Stop client at 9 seconds

    // 8. Simulation Setup
    ns3::Simulator::Stop(ns3::Seconds(10.0)); // Run simulation for 10 seconds

    // Optional: Enable PCAP tracing for network analysis
    // phy.EnablePcap("wifi-ap", apDevices.Get(0));
    // phy.EnablePcap("wifi-sta", staDevices.Get(0));

    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}