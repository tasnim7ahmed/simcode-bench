#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

int main(int argc, char *argv[])
{
    // Enable logging for relevant modules to see application output
    ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);
    // ns3::LogComponentEnable("WifiMac", ns3::LOG_LEVEL_INFO); // Uncomment for MAC layer details

    // Simulation parameters
    double simulationTime = 10.0; // seconds

    // 1. Create two nodes: Node 0 (AP) and Node 1 (STA)
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 2. Set mobility for nodes (ConstantPositionMobilityModel)
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Position Node 0 (AP) at (0.0, 0.0, 0.0)
    ns3::Ptr<ns3::ConstantPositionMobilityModel> apMobility = ns3::CreateObject<ns3::ConstantPositionMobilityModel>();
    apMobility->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    nodes.Get(0)->AggregateObject(apMobility);

    // Position Node 1 (STA) at (2.0, 2.0, 0.0) within a 2x2 meter grid
    ns3::Ptr<ns3::ConstantPositionMobilityModel> staMobility = ns3::CreateObject<ns3::ConstantPositionMobilityModel>();
    staMobility->SetPosition(ns3::Vector(2.0, 2.0, 0.0));
    nodes.Get(1)->AggregateObject(staMobility);

    // 3. Configure Wi-Fi communication using 802.11n standard
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211n); // Use 802.11n standard

    // Create Wi-Fi channel and PHY
    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationLoss("ns3::FriisPropagationLossModel");
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    ns3::Ptr<ns3::YansWifiChannel> wifiChannel = channel.Create();

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);
    // Optional: Set default transmit power for 802.11n devices
    // phy.Set("TxPowerStart", ns3::DoubleValue(20.0));
    // phy.Set("TxPowerEnd", ns3::DoubleValue(20.0));

    // Configure MAC layer for AP and STA
    ns3::WifiMacHelper mac;
    ns3::Ssid ssid = ns3::Ssid("ns3-80211n-network");

    // Install AP device on Node 0
    mac.SetType("ns3::ApWifiMac",
                "Ssid", ns3::SsidValue(ssid),
                "BeaconInterval", ns3::TimeValue(ns3::Seconds(0.1)));
    ns3::NetDeviceContainer apDevice = wifi.Install(phy, mac, nodes.Get(0));

    // Install STA device on Node 1
    mac.SetType("ns3::StaWifiMac",
                "Ssid", ns3::SsidValue(ssid),
                "ActiveProbing", ns3::BooleanValue(false)); // STA passively listens for beacons
    ns3::NetDeviceContainer staDevice = wifi.Install(phy, mac, nodes.Get(1));

    // 4. Install Internet Stack on both nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP addresses to the Wi-Fi devices
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0"); // Subnet 10.1.1.0/24

    ns3::Ipv4InterfaceContainer wifiInterfaces;
    wifiInterfaces = ipv4.Assign(apDevice);  // Node 0 gets 10.1.1.1
    wifiInterfaces.Add(ipv4.Assign(staDevice)); // Node 1 gets 10.1.1.2

    ns3::Ipv4Address apIpAddress = wifiInterfaces.GetAddress(0); // IP address of the AP (Node 0)

    // 6. Install UDP Echo Server on AP (Node 0)
    ns3::uint16_t port = 9; // Standard Echo port
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(ns3::Seconds(0.0)); // Server starts immediately
    serverApps.Stop(ns3::Seconds(simulationTime)); // Server runs for the duration of the simulation

    // 7. Install UDP Echo Client on STA (Node 1)
    ns3::UdpEchoClientHelper echoClient(apIpAddress, port);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(8)); // (10s sim time - 2s start time) / 1s interval = 8 packets
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // Send one packet every 1 second
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1024-byte packet
    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(ns3::Seconds(2.0)); // Client starts sending at 2 seconds
    clientApps.Stop(ns3::Seconds(simulationTime)); // Client stops at simulation end

    // Optional: Enable pcap tracing to capture network traffic
    // phy.EnablePcap("wifi-80211n-ap", apDevice.Get(0));
    // phy.EnablePcap("wifi-80211n-sta", staDevice.Get(0));

    // Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(simulationTime));

    // Run the simulation
    ns3::Simulator::Run();

    // Clean up simulation resources
    ns3::Simulator::Destroy();

    return 0;
}