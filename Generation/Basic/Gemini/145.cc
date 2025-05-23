#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"
#include <iostream> // For std::cout

int main(int argc, char *argv[])
{
    // 1. Create Nodes
    ns3::NodeContainer nodes;
    nodes.Create(3); // Node 0, Node 1, Node 2

    // 2. Set Static Mobility for each node
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    nodes.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(10.0, 0.0, 0.0));
    nodes.Get(2)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(5.0, 10.0, 0.0));

    // 3. Install Internet Stack on all nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // 4. Configure Wi-Fi Mesh Network
    ns3::WifiHelper wifi;
    // Set Wi-Fi standard to 802.11a (5GHz band, commonly used for mesh)
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211a);

    ns3::MeshHelper mesh;
    // Use a single channel for all mesh interfaces for simplicity
    mesh.SetSpreadInterfaceChannels(ns3::MeshHelper::SPREAD_CHANNELS_NONE);
    // Set MAC type for mesh nodes with a common SSID
    mesh.SetMacType("ns3::RandomStartWifiMac", "Ssid", ns3::Ssid("ns3-mesh-network"));
    // Set PHY type
    mesh.SetPhyType("ns3::WifiPhyHelper");

    // Configure the Wi-Fi channel and PHY
    ns3::YansWifiChannelHelper channel = ns3::YansWifiChannelHelper::Default();
    ns3::YansWifiPhyHelper phy = ns3::YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Install mesh devices on nodes
    ns3::NetDeviceContainer meshDevices = mesh.Install(phy, wifi, nodes);

    // 5. Assign IP Addresses to mesh devices
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer meshInterfaces = ipv4.Assign(meshDevices);

    // 6. Configure UDP Traffic Applications

    // Packet Sink (Receiver) on Node 0
    uint16_t port = 9;
    // Node 0's IP address will be 10.1.1.1 (first address in 10.1.1.0/24 subnet)
    ns3::Address sinkAddress(ns3::InetSocketAddress(meshInterfaces.GetAddress(0), port));
    ns3::PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ns3::ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(0));
    sinkApps.Start(ns3::Seconds(0.0));
    sinkApps.Stop(ns3::Seconds(10.0));

    // UDP Client 1 on Node 1, sending to Node 0
    ns3::UdpClientHelper clientHelper1(sinkAddress);
    clientHelper1.SetAttribute("MaxPackets", ns3::UintegerValue(10));     // Send 10 packets
    clientHelper1.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // One packet every second
    clientHelper1.SetAttribute("PacketSize", ns3::UintegerValue(100));    // 100 bytes per packet
    ns3::ApplicationContainer clientApps1 = clientHelper1.Install(nodes.Get(1));
    clientApps1.Start(ns3::Seconds(1.0)); // Start sending after 1 second
    clientApps1.Stop(ns3::Seconds(10.0));

    // UDP Client 2 on Node 2, sending to Node 0
    ns3::UdpClientHelper clientHelper2(sinkAddress);
    clientHelper2.SetAttribute("MaxPackets", ns3::UintegerValue(10));
    clientHelper2.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    clientHelper2.SetAttribute("PacketSize", ns3::UintegerValue(100));
    ns3::ApplicationContainer clientApps2 = clientHelper2.Install(nodes.Get(2));
    clientApps2.Start(ns3::Seconds(1.0)); // Start sending after 1 second
    clientApps2.Stop(ns3::Seconds(10.0));

    // 7. Enable PCAP Tracing
    // Captures all Wi-Fi PHY-level packets for all mesh devices
    phy.EnablePcapAll("mesh-wifi-trace");

    // 8. Set Simulation Duration and Run
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();

    // 9. Output Basic Statistics
    ns3::Ptr<ns3::PacketSink> sink = ns3::DynamicCast<ns3::PacketSink>(sinkApps.Get(0));
    std::cout << "\n--- Application Statistics ---" << std::endl;
    std::cout << "Node 0 (Sink) Total Bytes Received: " << sink->GetTotalRx() << std::endl;
    
    // For clients, we report the *intended* total bytes sent based on their configuration.
    // Each client is configured to send 10 packets * 100 bytes/packet = 1000 bytes.
    std::cout << "Node 1 (Client) Intended Bytes Sent: " << 10 * 100 << " (10 packets @ 100 bytes each)" << std::endl;
    std::cout << "Node 2 (Client) Intended Bytes Sent: " << 10 * 100 << " (10 packets @ 100 bytes each)" << std::endl;

    // Clean up simulation resources
    ns3::Simulator::Destroy();

    return 0;
}