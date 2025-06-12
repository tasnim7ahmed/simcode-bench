#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpWifiExample");

// Server function to receive packets
void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("Server received a packet over Wi-Fi!");
    }
}

// Client function to send packets
void SendPacket(Ptr<Socket> socket, Ipv4Address dstAddr, uint16_t port) {
    Ptr<Packet> packet = Create<Packet>(512); // 512-byte packet
    socket->SendTo(packet, 0, InetSocketAddress(dstAddr, port));
    Simulator::Schedule(Seconds(1.0), &SendPacket, socket, dstAddr, port); // Schedule next packet
}

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpWifiExample", LOG_LEVEL_INFO);

    // Create nodes (1 AP, 1 Client, 1 Server)
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer wifiClientNode, wifiServerNode;
    wifiClientNode.Create(1);
    wifiServerNode.Create(1);

    // Configure Wi-Fi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi MAC (Infrastructure Mode)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // Configure AP (Access Point)
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Configure Client and Server (Station Mode)
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer clientDevice = wifi.Install(phy, mac, wifiClientNode);
    NetDeviceContainer serverDevice = wifi.Install(phy, mac, wifiServerNode);

    // Set mobility model (static positions)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiClientNode);
    mobility.Install(wifiServerNode);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiClientNode);
    stack.Install(wifiServerNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer clientInterface = address.Assign(clientDevice);
    Ipv4InterfaceContainer serverInterface = address.Assign(serverDevice);

    uint16_t port = 8080; // UDP port

    // Setup UDP Server
    Ptr<Socket> serverSocket = Socket::CreateSocket(wifiServerNode.Get(0), UdpSocketFactory::GetTypeId());
    serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
    serverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Client
    Ptr<Socket> clientSocket = Socket::CreateSocket(wifiClientNode.Get(0), UdpSocketFactory::GetTypeId());
    Simulator::Schedule(Seconds(2.0), &SendPacket, clientSocket, serverInterface.GetAddress(0), port);

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

