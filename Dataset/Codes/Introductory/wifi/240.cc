#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpMulticastExample");

void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("Receiver received a UDP multicast packet!");
    }
}

int main() {
    LogComponentEnable("WifiUdpMulticastExample", LOG_LEVEL_INFO);

    uint16_t port = 5000;
    Ipv4Address multicastGroup("225.1.2.3");

    // Create nodes (1 sender, 2 receivers)
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> sender = nodes.Get(0);
    Ptr<Node> receiver1 = nodes.Get(1);
    Ptr<Node> receiver2 = nodes.Get(2);

    // Setup Wi-Fi Network
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc network

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility model (static nodes)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup Multicast Routing
    Ipv4StaticRoutingHelper multicastRouting;
    Ptr<Ipv4StaticRouting> receiver1Routing = multicastRouting.GetStaticRouting(receiver1->GetObject<Ipv4>());
    receiver1Routing->AddMulticastRoute(interfaces.GetAddress(0), multicastGroup, 1);

    Ptr<Ipv4StaticRouting> receiver2Routing = multicastRouting.GetStaticRouting(receiver2->GetObject<Ipv4>());
    receiver2Routing->AddMulticastRoute(interfaces.GetAddress(0), multicastGroup, 1);

    // Setup UDP Receiver (Multicast Group Members)
    Ptr<Socket> receiverSocket1 = Socket::CreateSocket(receiver1, UdpSocketFactory::GetTypeId());
    receiverSocket1->Bind(InetSocketAddress(multicastGroup, port));
    receiverSocket1->SetRecvCallback(MakeCallback(&ReceivePacket));

    Ptr<Socket> receiverSocket2 = Socket::CreateSocket(receiver2, UdpSocketFactory::GetTypeId());
    receiverSocket2->Bind(InetSocketAddress(multicastGroup, port));
    receiverSocket2->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Sender
    Ptr<Socket> senderSocket = Socket::CreateSocket(sender, UdpSocketFactory::GetTypeId());
    InetSocketAddress multicastAddr(multicastGroup, port);
    multicastAddr.SetTos(0xb8); // Best-effort traffic

    Simulator::Schedule(Seconds(1.0), &Socket::SendTo, senderSocket, Create<Packet>(128), 0, multicastAddr);
    Simulator::Schedule(Seconds(2.0), &Socket::SendTo, senderSocket, Create<Packet>(128), 0, multicastAddr);

    // Run the simulation
    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

