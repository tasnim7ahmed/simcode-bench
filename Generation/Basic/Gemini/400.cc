#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    // Set default simulation time
    double simulationTime = 10.0; // seconds

    // Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // Install mobility model for static nodes
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    nodes.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(1.0, 0.0, 0.0));

    // Install the Internet stack on the nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // Configure Wi-Fi
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n); // Use 802.11n standard

    // Set up YansWifiChannel (propagation model)
    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationLoss("ns3::YansPropagationLossModel");
    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Set up MAC (AdhocWifiMac as no AP is specified)
    ns3::WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac", "Ssid", ns3::SsidValue("my-wifi-network"));

    // Install Wi-Fi devices
    ns3::NetDeviceContainer devices;
    devices = wifi.Install(phy, mac, nodes);

    // Assign IP addresses
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Get server and client IP addresses
    ns3::Ipv4Address serverAddress = interfaces.GetAddress(1); // Node 1 is server

    // Setup TCP Server (PacketSink application)
    uint16_t port = 9; // Server listens on port 9
    ns3::PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                     ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer serverApps = sinkHelper.Install(nodes.Get(1)); // Server is Node 1
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(simulationTime + 1.0)); // Stop slightly after simulation end

    // Setup TCP Client (OnOffApplication)
    ns3::OnOffHelper onoffHelper("ns3::TcpSocketFactory",
                                 ns3::InetSocketAddress(serverAddress, port));
    onoffHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoffHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoffHelper.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1024 bytes
    // Calculate data rate: 1 packet (1024 bytes) every 100ms (0.1s)
    // Rate = (1024 bytes * 8 bits/byte) / 0.1 s = 81920 bps
    onoffHelper.SetAttribute("DataRate", ns3::DataRateValue("81920bps"));
    onoffHelper.SetAttribute("MaxBytes", ns3::UintegerValue(0)); // Send indefinitely

    ns3::ApplicationContainer clientApps = onoffHelper.Install(nodes.Get(0)); // Client is Node 0
    clientApps.Start(ns3::Seconds(1.0)); // Start client after server is ready
    clientApps.Stop(ns3::Seconds(simulationTime));

    // Run the simulation
    ns3::Simulator::Stop(ns3::Seconds(simulationTime + 1.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}