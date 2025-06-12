#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MinimalUdpExample");

// Function to receive packets at the server
void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("Server received a packet!");
    }
}

int main() {
    LogComponentEnable("MinimalUdpExample", LOG_LEVEL_INFO);

    // Create two nodes (client and server)
    NodeContainer nodes;
    nodes.Create(2);

    // Set up a point-to-point connection
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 4000; // UDP port

    // Setup UDP Server on node 1
    Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
    serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
    serverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Client on node 0
    Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
    Ptr<Packet> packet = Create<Packet>(128); // 128-byte packet
    Simulator::Schedule(Seconds(1.0), &Socket::SendTo, clientSocket, packet, 0, InetSocketAddress(interfaces.GetAddress(1), port));

    // Run the simulation
    Simulator::Stop(Seconds(2.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

