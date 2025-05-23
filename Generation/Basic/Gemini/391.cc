#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

int
main(int argc, char* argv[])
{
    // 1. Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 2. Set node positions using ConstantPositionMobilityModel
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    nodes.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0)); // Sender
    nodes.Get(1)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(1.0, 1.0, 0.0)); // Receiver

    // 3. Install Internet Stack
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // 4. Setup Wi-Fi
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n_5GHZ); // Or other standard like 80211a

    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationLoss("ns3::LogDistancePropagationLossModel",
                               "Exponent", ns3::DoubleValue(3.0));
    channel.SetReceiveErrorModel("ns3::NistErrorRateModel");
    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    ns3::Ssid ssid = ns3::Ssid("wifi-ssid");
    ns3::NqosWifiMacHelper apWifiMac = ns3::NqosWifiMacHelper::Default();
    ns3::NqosWifiMacHelper staWifiMac = ns3::NqosWifiMacHelper::Default();

    // Configure MAC for AP and STA
    apWifiMac.SetType("ns3::ApWifiMac", "Ssid", ns3::SsidValue(ssid), "BeaconInterval", ns3::TimeValue(ns3::MicroSeconds(1024)));
    staWifiMac.SetType("ns3::StaWifiMac", "Ssid", ns3::SsidValue(ssid));

    // Install Wi-Fi devices
    ns3::NetDeviceContainer apDevice = wifi.Install(phy, apWifiMac, nodes.Get(1));  // Node 1 as AP
    ns3::NetDeviceContainer staDevice = wifi.Install(phy, staWifiMac, nodes.Get(0)); // Node 0 as STA

    // 5. Assign IP addresses
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apDevice);
    ns3::Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevice);

    // 6. Setup UDP Applications (Sender to Receiver)
    uint16_t port = 9; // Echo port

    // Receiver (Packet Sink)
    ns3::UdpServerHelper server(port);
    ns3::ApplicationContainer serverApps = server.Install(nodes.Get(1)); // Node 1 is receiver
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(9.0));

    // Sender (UDP Client)
    ns3::UdpClientHelper client(apInterfaces.GetAddress(0), port); // Send to Node 1's IP
    client.SetAttribute("MaxPackets", ns3::UintegerValue(100)); // Send 100 packets
    client.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(0.1))); // Send every 0.1 seconds
    client.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // Packet size 1024 bytes
    ns3::ApplicationContainer clientApps = client.Install(nodes.Get(0)); // Node 0 is sender
    clientApps.Start(ns3::Seconds(2.0));
    clientApps.Stop(ns3::Seconds(9.0));

    // 7. Run Simulation
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}