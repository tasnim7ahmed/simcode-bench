#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWifiExample");

int main() {
    LogComponentEnable("TcpWifiExample", LOG_LEVEL_INFO);

    int numClients = 3;
    uint16_t port = 5000;

    // Create nodes (clients + 1 server)
    NodeContainer wifiNodes;
    wifiNodes.Create(numClients + 1);

    NodeContainer serverNode = wifiNodes.Get(numClients); // Last node is the server

    // Set up Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("tcp-wifi-network");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer clientDevices = wifi.Install(phy, mac, wifiNodes.Get(0, numClients));

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer serverDevice = wifi.Install(phy, mac, serverNode);

    // Mobility model (static positions)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(clientDevices);
    Ipv4InterfaceContainer serverInterface = address.Assign(serverDevice);

    // Setup TCP Server
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sink.Install(serverNode);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(5.0));

    // Setup TCP Clients
    for (int i = 0; i < numClients; i++) {
        OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(serverInterface.GetAddress(0), port));
        client.SetConstantRate(DataRate("1Mbps"), 512); // Send 512-byte packets
        ApplicationContainer clientApp = client.Install(wifiNodes.Get(i));
        clientApp.Start(Seconds(1.0 + i));
        clientApp.Stop(Seconds(5.0));
    }

    // Run the simulation
    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

