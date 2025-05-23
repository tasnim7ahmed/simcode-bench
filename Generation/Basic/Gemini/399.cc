#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

// NS_LOG_COMPONENT_DEFINE ("WifiTwoNodeCbr"); // Optional: for logging

int main(int argc, char* argv[])
{
    // Enable logging for specific components (optional)
    // LogComponentEnable ("WifiTwoNodeCbr", LOG_LEVEL_INFO);
    // LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(2); // node 0 for AP/Server, node 1 for STA/Client

    // 2. Configure Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // 3. Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n); // Using 802.11n for modern Wi-Fi setup

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // Install Wi-Fi on AP (Node 0)
    mac.SetType("ns3::Mac80211ApHelper",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, nodes.Get(0));

    // Install Wi-Fi on STA (Node 1)
    mac.SetType("ns3::Mac80211StaHelper",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false)); // Disable active probing for simplicity
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, nodes.Get(1));

    // 4. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(apDevices);
    interfaces.Add(address.Assign(staDevices));

    // Get the server's IP address (AP's IP)
    Ipv4Address serverAddress = interfaces.GetAddress(0); // Assuming AP gets the first IP in the subnet

    // 6. Configure Applications
    uint16_t port = 9; // Server listens on port 9

    // UDP Server on Node 0 (AP)
    PacketSinkHelper serverSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = serverSink.Install(nodes.Get(0));
    serverApps.Start(Seconds(0.0)); // Start server immediately
    serverApps.Stop(Seconds(10.0)); // Stop server at end of simulation

    // UDP Client (CBR) on Node 1 (STA)
    OnOffHelper clientSource("ns3::UdpSocketFactory", InetSocketAddress(serverAddress, port));
    clientSource.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));  // Always On
    clientSource.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // Never Off
    clientSource.SetAttribute("PacketSize", UintegerValue(1024)); // Packet size: 1024 bytes

    // Calculate DataRate for 1024 bytes every 100ms
    // 100ms interval means 10 packets per second
    // Data rate = (1024 bytes/packet) * (8 bits/byte) * (10 packets/second) = 81920 bits/second
    clientSource.SetAttribute("DataRate", DataRateValue(81920)); // DataRate is in bps

    ApplicationContainer clientApps = clientSource.Install(nodes.Get(1));
    clientApps.Start(Seconds(1.0)); // Start client after 1 second
    clientApps.Stop(Seconds(9.0)); // Stop client before simulation ends to allow final packets to arrive

    // 7. Simulation Control
    Simulator::Stop(Seconds(10.0)); // Run simulation for 10 seconds
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}