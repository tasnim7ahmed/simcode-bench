#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpClientServer");

void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("AP received a UDP packet!");
    }
}

int main() {
    LogComponentEnable("WifiUdpClientServer", LOG_LEVEL_INFO);

    uint16_t port = 5000;

    // Create nodes (client and access point)
    NodeContainer wifiNodes;
    wifiNodes.Create(2);
    Ptr<Node> client = wifiNodes.Get(0);
    Ptr<Node> ap = wifiNodes.Get(1);

    // Set up Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("wifi-network");

    // Configure AP
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, ap);

    // Configure Client
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer clientDevice = wifi.Install(wifiPhy, wifiMac, client);

    // Mobility (static positions)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(clientDevice, apDevice));

    // Setup UDP Server (AP)
    Ptr<Socket> apSocket = Socket::CreateSocket(ap, UdpSocketFactory::GetTypeId());
    apSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
    apSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Client
    Ptr<Socket> clientSocket = Socket::CreateSocket(client, UdpSocketFactory::GetTypeId());
    InetSocketAddress apAddress(interfaces.GetAddress(1), port);

    Simulator::Schedule(Seconds(1.0), &Socket::SendTo, clientSocket, Create<Packet>(64), 0, apAddress);
    Simulator::Schedule(Seconds(2.0), &Socket::SendTo, clientSocket, Create<Packet>(64), 0, apAddress);

    // Run the simulation
    Simulator::Stop(Seconds(3.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

