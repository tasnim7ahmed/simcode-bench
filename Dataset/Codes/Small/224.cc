#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include <cstdlib>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpManetExample");

// Function to send UDP packets to a random neighbor
void SendPacket(Ptr<Socket> socket, Ipv4InterfaceContainer &interfaces) {
    int numNodes = interfaces.GetN();
    int targetNode = rand() % numNodes; // Choose a random target node

    Ipv4Address dstAddr = interfaces.GetAddress(targetNode);
    Ptr<Packet> packet = Create<Packet>(512); // 512-byte packet
    socket->SendTo(packet, 0, InetSocketAddress(dstAddr, 8080));

    Simulator::Schedule(Seconds(2.0), &SendPacket, socket, interfaces); // Schedule next packet
}

// Function to receive packets
void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("Node received a packet in MANET!");
    }
}

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpManetExample", LOG_LEVEL_INFO);

    int numNodes = 10;
    double simTime = 20.0;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Configure Wi-Fi (Ad-Hoc Mode)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Set up mobility (Random Waypoint Model)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=10.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
    mobility.Install(nodes);

    // Install Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP sockets
    for (int i = 0; i < numNodes; i++) {
        Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
        recvSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 8080));
        recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        Ptr<Socket> sendSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
        Simulator::Schedule(Seconds(2.0 + i * 0.5), &SendPacket, sendSocket, interfaces);
    }

    // Run the simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

