#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    // Create four nodes
    ns3::NodeContainer nodes;
    nodes.Create(4);

    // Configure Wi-Fi PHY and Channel
    ns3::YansWifiPhyHelper wifiPhy;
    ns3::YansWifiChannelHelper wifiChannel = ns3::YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.SetPcapDataLinkType(ns3::WifiPhyHelper::DLT_IEEE802_11_RADIO);

    // Configure 802.11s Mesh MAC
    ns3::MeshHelper mesh;
    mesh.SetStackInstaller("ns3::dot11sStackHelper"); // Use 802.11s mesh stack
    mesh.SetSpreadInterfaceChannels(ns3::MeshHelper::ZERO_CHANNEL); // All nodes on the same channel
    // Set a constant rate manager for simplicity and determinism
    mesh.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataRate", ns3::StringValue("OfdmRate54Mbps"),
                                 "RtsCtsThreshold", ns3::UintegerValue(2000)); // Disable RTS/CTS for packets under 2000 bytes

    // Configure WifiMacHelper for 802.11s
    ns3::WifiMacHelper wifiMac = ns3::WifiMacHelper::Default();
    wifiMac.SetType("ns3::Mac80211sHelper");

    // Configure Mobility - static grid layout for linear arrangement
    ns3::MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", ns3::DoubleValue(0.0),
                                  "MinY", ns3::DoubleValue(0.0),
                                  "DeltaX", ns3::DoubleValue(5.0), // Nodes arranged horizontally, 5m apart
                                  "DeltaY", ns3::DoubleValue(0.0),
                                  "GridWidth", ns3::UintegerValue(4), // 4 nodes in a row
                                  "LayoutType", ns3::StringValue("ROW_FIRST"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Wi-Fi Mesh devices on nodes
    ns3::NetDeviceContainer meshDevices = mesh.Install(wifiPhy, wifiMac, nodes);

    // Install Internet Stack on nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP Addresses to mesh devices
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer ipv4Ifaces = ipv4.Assign(meshDevices);

    // Setup UDP Server on the last node (Node 3)
    uint16_t port = 9;
    ns3::Address serverAddress(ipv4Ifaces.GetAddress(3, 0)); // IP address of node 3
    ns3::PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                           ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer serverApps = packetSinkHelper.Install(nodes.Get(3));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0)); // Server stops at simulation end

    // Setup UDP Clients on the first three nodes (Nodes 0, 1, 2)
    uint32_t packetSize = 1024;
    uint32_t numPackets = 1000;
    ns3::Time interPacketInterval = ns3::Seconds(0.01); // 10 ms interval

    for (uint32_t i = 0; i < 3; ++i) // Clients on nodes 0, 1, 2
    {
        ns3::UdpEchoClientHelper echoClientHelper(serverAddress.Create<ns3::InetSocketAddress>().GetIpv4(), port);
        echoClientHelper.SetAttribute("MaxPackets", ns3::UintegerValue(numPackets));
        echoClientHelper.SetAttribute("Interval", ns3::TimeValue(interPacketInterval));
        echoClientHelper.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));

        ns3::ApplicationContainer clientApps = echoClientHelper.Install(nodes.Get(i));
        clientApps.Start(ns3::Seconds(2.0));
        // Clients will attempt to send 1000 packets. Since the simulation stops at 10s and clients start at 2s,
        // they will effectively send packets for 8 seconds.
        // Number of packets sent = 8 seconds / 0.01 seconds/packet = 800 packets.
        // If all 1000 packets must be sent, the simulation time would need to be extended to 12 seconds.
        clientApps.Stop(ns3::Seconds(10.0)); // Clients stop at simulation end
    }

    // Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // Run simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}