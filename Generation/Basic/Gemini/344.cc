#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    // Create three nodes
    NodeContainer nodes;
    nodes.Create(3); // Node 0, Node 1, Node 2

    // Configure mobility: Statically placed with a grid-based mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(1.0),
                                  "DeltaY", DoubleValue(1.0),
                                  "GridWidth", UintegerValue(3), // Arrange nodes in a 3x1 row
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel"); // Nodes are static
    mobility.Install(nodes);
    // This will place Node 0 at (0,0,0), Node 1 at (1,0,0), Node 2 at (2,0,0)

    // Configure WiFi devices (802.11b)
    WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211b); // Set 802.11b standard

    YansWifiPhyHelper phy;
    phy.SetChannel(YansWifiChannelHelper::Create()); // Create a channel
    
    // Set up MAC layer for Ad-hoc mode
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Install Internet stack and assign IP addresses
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Get IP address of Node 2 (server)
    Ipv4Address serverAddress = interfaces.GetAddress(2); // Node 2 is the 3rd device (index 2)
    uint16_t serverPort = 9; // TCP server port

    // Configure TCP Server on Node 2
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    ApplicationContainer serverApps = sinkHelper.Install(nodes.Get(2)); // Node 2
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // Configure TCP Client on Node 0
    OnOffHelper client0Helper("ns3::TcpSocketFactory",
                              InetSocketAddress(serverAddress, serverPort));
    client0Helper.SetAttribute("PacketSize", UintegerValue(512));
    // Data rate: 512 bytes/packet * 8 bits/byte / 0.1 seconds/packet = 40960 bps
    client0Helper.SetAttribute("DataRate", DataRateValue(DataRate("40960bps")));
    client0Helper.SetAttribute("MaxBytes", UintegerValue(512 * 10)); // 10 packets of 512 bytes

    ApplicationContainer client0Apps = client0Helper.Install(nodes.Get(0)); // Node 0
    client0Apps.Start(Seconds(1.0));
    client0Apps.Stop(Seconds(2.0)); // Clients will finish sending by 1.9s (1.0s + 0.9s for 10 packets)

    // Configure TCP Client on Node 1
    OnOffHelper client1Helper("ns3::TcpSocketFactory",
                              InetSocketAddress(serverAddress, serverPort));
    client1Helper.SetAttribute("PacketSize", UintegerValue(512));
    client1Helper.SetAttribute("DataRate", DataRateValue(DataRate("40960bps"))); // 512 bytes / 0.1s = 40960 bps
    client1Helper.SetAttribute("MaxBytes", UintegerValue(512 * 10)); // 10 packets of 512 bytes

    ApplicationContainer client1Apps = client1Helper.Install(nodes.Get(1)); // Node 1
    client1Apps.Start(Seconds(1.0));
    client1Apps.Stop(Seconds(2.0)); // Clients will finish sending by 1.9s (1.0s + 0.9s for 10 packets)

    // Set simulation run time
    Simulator::Stop(Seconds(20.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}