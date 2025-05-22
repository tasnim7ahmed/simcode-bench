#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpSocketExample");

// Server function to receive packets
void ReceivePacket(Ptr<Socket> socket)
{
    while (socket->Recv())
    {
        NS_LOG_INFO("Server received a packet!");
    }
}

// Client function to send packets
void SendPacket(Ptr<Socket> socket, Ipv4Address dstAddr, uint16_t port)
{
    Ptr<Packet> packet = Create<Packet>(1024); // 1024-byte packet
    socket->SendTo(packet, 0, InetSocketAddress(dstAddr, port));
    Simulator::Schedule(Seconds(1.0), &SendPacket, socket, dstAddr, port); // Schedule next packet
}

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpSocketExample", LOG_LEVEL_INFO);

    // Create nodes (Client and Server)
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 8080; // UDP port

    // Setup UDP Server
    Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
    serverSocket->Bind(local);
    serverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Client
    Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
    Simulator::Schedule(Seconds(2.0), &SendPacket, clientSocket, interfaces.GetAddress(1), port);

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
