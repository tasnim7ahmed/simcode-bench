#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpBroadcastExample");

// Function to log received packets
void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("Receiver received a broadcast packet!");
    }
}

int main() {
    LogComponentEnable("UdpBroadcastExample", LOG_LEVEL_INFO);

    int numReceivers = 3;
    uint16_t port = 4000;

    // Create nodes (1 sender + multiple receivers)
    NodeContainer wifiNodes;
    wifiNodes.Create(numReceivers + 1);

    NodeContainer senderNode = wifiNodes.Get(0); // First node is the sender

    // Set up Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("udp-broadcast-network");
    mac.SetType("ns3::AdhocWifiMac"); // Ad hoc mode for broadcasting

    NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

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
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP Broadcast Sender
    Ptr<Socket> senderSocket = Socket::CreateSocket(senderNode, UdpSocketFactory::GetTypeId());
    Ipv4Address broadcastAddress("192.168.1.255"); // Broadcast address
    InetSocketAddress remote = InetSocketAddress(broadcastAddress, port);

    // Schedule broadcast packet transmission
    Simulator::Schedule(Seconds(1.0), &Socket::SendTo, senderSocket, Create<Packet>(128), 0, remote);

    // Setup Receivers
    for (int i = 1; i <= numReceivers; i++) {
        Ptr<Socket> receiverSocket = Socket::CreateSocket(wifiNodes.Get(i), UdpSocketFactory::GetTypeId());
        receiverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        receiverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));
    }

    // Run the simulation
    Simulator::Stop(Seconds(2.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

