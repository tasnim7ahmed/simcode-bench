#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpMultiServerExample");

// Server function to receive packets
void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("Server received a packet!");
    }
}

// Client function to send packets to a specific server
void SendPacket(Ptr<Socket> socket, Ipv4Address dstAddr, uint16_t port) {
    Ptr<Packet> packet = Create<Packet>(512); // 512-byte packet
    socket->SendTo(packet, 0, InetSocketAddress(dstAddr, port));
    Simulator::Schedule(Seconds(1.0), &SendPacket, socket, dstAddr, port); // Schedule next packet
}

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpMultiServerExample", LOG_LEVEL_INFO);

    // Create nodes (1 Client + 2 Servers)
    NodeContainer nodes;
    nodes.Create(3);

    // Configure CSMA network (Ethernet-like)
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(5000)));

    NetDeviceContainer devices = csma.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port1 = 8080; // Server 1 port
    uint16_t port2 = 9090; // Server 2 port

    // Setup UDP Server 1
    Ptr<Socket> serverSocket1 = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
    serverSocket1->Bind(InetSocketAddress(Ipv4Address::GetAny(), port1));
    serverSocket1->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Server 2
    Ptr<Socket> serverSocket2 = Socket::CreateSocket(nodes.Get(2), UdpSocketFactory::GetTypeId());
    serverSocket2->Bind(InetSocketAddress(Ipv4Address::GetAny(), port2));
    serverSocket2->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Client
    Ptr<Socket> clientSocket1 = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
    Ptr<Socket> clientSocket2 = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());

    // Schedule packet sending to both servers
    Simulator::Schedule(Seconds(2.0), &SendPacket, clientSocket1, interfaces.GetAddress(1), port1);
    Simulator::Schedule(Seconds(2.5), &SendPacket, clientSocket2, interfaces.GetAddress(2), port2);

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

