#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

int
main(int argc, char* argv[])
{
    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(2); // Node 0: AP, Node 1: Station

    // 2. Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n); // Using 802.11n standard

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    Ptr<YansWifiChannel> wifiChannel = channel.Create();
    phy.SetChannel(wifiChannel);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    // Configure AP MAC
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, nodes.Get(0)); // Node 0 is AP

    // Configure Station MAC
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, nodes.Get(1)); // Node 1 is Station

    // 3. Set Mobility (required for Wi-Fi)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // 4. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    // 6. Configure UDP Applications
    // Server (on AP, Node 0)
    uint16_t port = 9; // Use discard port
    UdpServerHelper serverHelper(port);
    ApplicationContainer serverApp = serverHelper.Install(nodes.Get(0)); // Install on AP (Node 0)
    serverApp.Start(Seconds(1.0)); // Start server before client
    serverApp.Stop(Seconds(10.0)); // Stop at simulation end

    // Client (on Station, Node 1)
    // Send UDP packets to AP's IP address and port
    UdpClientHelper clientHelper(apInterface.GetAddress(0), port);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(1000)); // Max packets to send (arbitrary high)
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Send a packet every 0.1 seconds (10 packets/sec)
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024)); // Packet size of 1024 bytes
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(1)); // Install on Station (Node 1)
    clientApp.Start(Seconds(2.0)); // Start sending at 2 seconds
    clientApp.Stop(Seconds(10.0)); // Stop sending at 10 seconds

    // 7. Set Simulation Duration
    Simulator::Stop(Seconds(10.0));

    // 8. Run Simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}