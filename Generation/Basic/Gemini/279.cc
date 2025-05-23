#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

// Do not use the ns3 namespace directly, use ns3:: prefixes
// for clarity and to avoid potential conflicts with other libraries.

int main(int argc, char *argv[])
{
    ns3::CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes
    ns3::NodeContainer apNode;
    apNode.Create(1);
    ns3::NodeContainer staNodes;
    staNodes.Create(3);

    // Set up mobility for AP
    ns3::MobilityHelper mobilityAp;
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    ns3::Ptr<ns3::ListPositionAllocator> positionAlloc = ns3::CreateObject<ns3::ListPositionAllocator>();
    positionAlloc->Add(ns3::Vector(0.0, 0.0, 0.0)); // AP at origin
    mobilityAp.SetPositionAllocator(positionAlloc);
    mobilityAp.Install(apNode);

    // Set up mobility for STAs
    ns3::MobilityHelper mobilitySta;
    // Initial random position within the 50x50 area
    mobilitySta.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                      "X", ns3::StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                      "Y", ns3::StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    // Random Walk mobility within the 50x50 area
    mobilitySta.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", ns3::RectangleValue(ns3::Rectangle(0, 50, 0, 50)),
                                  "Distance", ns3::DoubleValue(5.0), // Walk 5 meters before changing direction
                                  "Speed", ns3::StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]")); // Speed between 1 and 2 m/s
    mobilitySta.Install(staNodes);

    // Install internet stack
    ns3::InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    // Configure Wi-Fi devices
    ns3::WifiMacHelper wifiMac;
    ns3::WifiHelper wifi;
    ns3::Ssid ssid = ns3::Ssid("my-wifi");

    // Set up Wi-Fi channel and PHY
    ns3::YansWifiPhyHelper phy;
    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    channel.SetSpectrumPropagationLossModel("ns3::SpectrumPropagationLossModel");
    phy.SetChannel(channel.Create());

    // Configure AP MAC and install device
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211n_2_4GHZ); // Example Wi-Fi standard
    wifi.SetRemoteStationManager("ns3::AarfWifiManager"); // Example rate adaptation algorithm
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", ns3::SsidValue(ssid));
    ns3::NetDeviceContainer apDevices = wifi.Install(phy, wifiMac, apNode);

    // Configure STA MACs and install devices
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", ns3::SsidValue(ssid));
    ns3::NetDeviceContainer staDevices = wifi.Install(phy, wifiMac, staNodes);

    // Assign IP addresses
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer apIpInterface = ipv4.Assign(apDevices);
    ns3::Ipv4InterfaceContainer staIpInterfaces = ipv4.Assign(staDevices);

    // Set up UDP Server on AP
    ns3::UdpServerHelper server(9); // Port 9
    ns3::ApplicationContainer serverApps = server.Install(apNode.Get(0));
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // Set up UDP Clients on STAs
    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        ns3::UdpClientHelper client(apIpInterface.GetAddress(0), 9);
        client.SetAttribute("MaxPackets", ns3::UintegerValue(5));
        client.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1)));
        client.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1024 bytes packet size

        ns3::ApplicationContainer clientApps = client.Install(staNodes.Get(i));
        clientApps.Start(ns3::Seconds(1.0 + i * 0.1)); // Stagger client starts slightly
        clientApps.Stop(ns3::Seconds(10.0));
    }

    // Enable PCAP tracing for network devices (optional, useful for debugging)
    // phy.EnablePcap("wifi-ap", apDevices.Get(0));
    // phy.EnablePcap("wifi-sta", staDevices);

    // Run simulation
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}