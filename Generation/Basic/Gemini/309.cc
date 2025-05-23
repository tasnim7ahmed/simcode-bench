#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

int
main(int argc, char* argv[])
{
    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // 1. Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 2. Configure Wi-Fi for 802.11b
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211b);

    ns3::YansWifiPhyHelper phy;
    // Set up a channel
    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationLoss("ns3::FriisPropagationLossModel");
    channel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", ns3::DoubleValue(100.0));
    phy.SetChannel(channel.Create());

    // Explicitly set the transmission power if needed for range, though defaults might suffice
    // phy.Set("TxPowerStart", ns3::DoubleValue(7.0));  // Example: 7 dBm
    // phy.Set("TxPowerEnd", ns3::DoubleValue(7.0));    // Example: 7 dBm

    ns3::WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac"); // Ad-hoc mode for simplicity (no AP needed)

    // Set a constant rate for 802.11b (e.g., 1 Mbps)
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", ns3::StringValue("ns3::DsssRate1Mbps"),
                                 "ControlMode", ns3::StringValue("ns3::DsssRate1Mbps"));

    ns3::NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // 3. Set fixed positions for nodes
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set positions for sender (node 0) and receiver (node 1)
    nodes.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(10.0, 0.0, 0.0)); // 10m apart

    // 4. Install Internet Stack
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP Addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 6. Setup UDP Server
    uint16_t port = 8080;
    ns3::UdpServerHelper server(port);
    ns3::ApplicationContainer serverApps = server.Install(nodes.Get(1)); // Receiver is node 1
    serverApps.Start(ns3::Seconds(1.0)); // Receiver starts at 1 second
    serverApps.Stop(ns3::Seconds(10.0));

    // 7. Setup UDP Client
    ns3::Ipv4Address serverAddress = interfaces.GetAddress(1); // IP of the receiver (node 1)
    ns3::UdpClientHelper client(serverAddress, port);
    client.SetAttribute("MaxPackets", ns3::UintegerValue(1000));      // 1000 packets
    client.SetAttribute("Interval", ns3::TimeValue(ns3::MilliSeconds(10))); // 100 packets/sec (10ms interval)
    client.SetAttribute("PacketSize", ns3::UintegerValue(512));       // 512 bytes per packet

    ns3::ApplicationContainer clientApps = client.Install(nodes.Get(0)); // Sender is node 0
    clientApps.Start(ns3::Seconds(2.0)); // Sender starts at 2 seconds
    clientApps.Stop(ns3::Seconds(10.0));

    // 8. Run Simulation
    ns3::Simulator::Stop(ns3::Seconds(10.0)); // Simulation runs for 10 seconds
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}