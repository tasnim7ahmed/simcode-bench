#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("TcpClient", LOG_LEVEL_INFO);
    LogComponentEnable("TcpServer", LOG_LEVEL_INFO);

    // Create two nodes (Client and Server)
    NodeContainer nodes;
    nodes.Create(2);

    // Set up WiFi channel and MAC layer
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    // Set default bitrate for 802.11b
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    // Configure MAC layer
    Ssid ssid = Ssid("ns-3-ssid");
    WifiMacHelper mac;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

    WifiMacHelper apMac;
    apMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

    // Install PHY and MAC layers
    WifiPhyHelper phy;
    phy.Set("ChannelWidth", UintegerValue(20));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, nodes.Get(0));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, apMac, nodes.Get(1));

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(apDevices);
    interfaces.Add(address.Assign(staDevices));

    // Set up TCP server on node 1
    uint16_t port = 9;  // TCP port number
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP client on node 0
    TcpClientHelper tcpClient(interfaces.GetAddress(1), port);
    ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}