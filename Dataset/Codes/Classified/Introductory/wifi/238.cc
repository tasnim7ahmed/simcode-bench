#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiUdpExample");

void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("Server received a UDP packet!");
    }
}

int main() {
    LogComponentEnable("AdhocWifiUdpExample", LOG_LEVEL_INFO);

    uint16_t port = 5001;

    // Create nodes (mobile client and static server)
    NodeContainer nodes;
    nodes.Create(2);
    Ptr<Node> mobileNode = nodes.Get(0);
    Ptr<Node> staticNode = nodes.Get(1);

    // Configure Ad-hoc Wi-Fi (802.11s)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211s);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility model (mobile node moves, static node stays)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel", "Position", Vector3DValue(Vector3D(0, 0, 0)));
    mobility.Install(staticNode);

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(mobileNode);
    mobileNode->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(5.0, 0.0, 0.0)); // Moves at 5 m/s

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP Server (Static Node)
    Ptr<Socket> serverSocket = Socket::CreateSocket(staticNode, UdpSocketFactory::GetTypeId());
    serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
    serverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Client (Mobile Node)
    Ptr<Socket> clientSocket = Socket::CreateSocket(mobileNode, UdpSocketFactory::GetTypeId());
    InetSocketAddress serverAddress(interfaces.GetAddress(1), port);

    Simulator::Schedule(Seconds(1.0), &Socket::SendTo, clientSocket, Create<Packet>(128), 0, serverAddress);
    Simulator::Schedule(Seconds(2.0), &Socket::SendTo, clientSocket, Create<Packet>(128), 0, serverAddress);

    // Run the simulation
    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

