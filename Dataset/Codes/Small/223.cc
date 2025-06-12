#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpAdhocWifiExample");

// Server function to receive packets
void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("Server received a packet over Wi-Fi Ad-Hoc!");
    }
}

// Client function to send packets
void SendPacket(Ptr<Socket> socket, Ipv4Address dstAddr, uint16_t port) {
    Ptr<Packet> packet = Create<Packet>(512); // 512-byte packet
    socket->SendTo(packet, 0, InetSocketAddress(dstAddr, port));
    Simulator::Schedule(Seconds(1.0), &SendPacket, socket, dstAddr, port); // Schedule next packet
}

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpAdhocWifiExample", LOG_LEVEL_INFO);

    // Create nodes (1 Client, 1 Server)
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Configure Wi-Fi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi MAC (Ad-Hoc Mode)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

    // Set mobility model (nodes move randomly)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", 
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiNodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 8080; // UDP port

    // Setup UDP Server
    Ptr<Socket> serverSocket = Socket::CreateSocket(wifiNodes.Get(1), UdpSocketFactory::GetTypeId());
    serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
    serverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Client
    Ptr<Socket> clientSocket = Socket::CreateSocket(wifiNodes.Get(0), UdpSocketFactory::GetTypeId());
    Simulator::Schedule(Seconds(2.0), &SendPacket, clientSocket, interfaces.GetAddress(1), port);

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

