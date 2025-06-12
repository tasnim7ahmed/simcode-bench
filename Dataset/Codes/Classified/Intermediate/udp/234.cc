#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpRelayExample");

// Relay function: Forward received packets to the server
void RelayPacket(Ptr<Socket> relaySocket, Address serverAddress) {
    Ptr<Packet> packet;
    while ((packet = relaySocket->Recv())) {
        NS_LOG_INFO("Relay received a packet, forwarding to server...");
        relaySocket->SendTo(packet, 0, serverAddress);
    }
}

int main() {
    LogComponentEnable("UdpRelayExample", LOG_LEVEL_INFO);

    uint16_t clientPort = 4000, serverPort = 5000;

    // Create nodes (client, relay, server)
    NodeContainer nodes;
    nodes.Create(3);
    NodeContainer client = NodeContainer(nodes.Get(0));
    NodeContainer relay = NodeContainer(nodes.Get(1));
    NodeContainer server = NodeContainer(nodes.Get(2));

    // Create point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer clientToRelay = pointToPoint.Install(client.Get(0), relay.Get(0));
    NetDeviceContainer relayToServer = pointToPoint.Install(relay.Get(0), server.Get(0));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer clientIface = address.Assign(clientToRelay);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer serverIface = address.Assign(relayToServer);

    // Setup UDP Server
    Ptr<Socket> serverSocket = Socket::CreateSocket(server.Get(0), UdpSocketFactory::GetTypeId());
    serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    serverSocket->SetRecvCallback([](Ptr<Socket> socket) {
        while (socket->Recv()) {
            NS_LOG_INFO("Server received a forwarded packet!");
        }
    });

    // Setup UDP Relay
    Ptr<Socket> relaySocket = Socket::CreateSocket(relay.Get(0), UdpSocketFactory::GetTypeId());
    relaySocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), clientPort));
    relaySocket->SetRecvCallback(MakeBoundCallback(&RelayPacket, relaySocket, InetSocketAddress(serverIface.GetAddress(1), serverPort)));

    // Setup UDP Client
    Ptr<Socket> clientSocket = Socket::CreateSocket(client.Get(0), UdpSocketFactory::GetTypeId());
    InetSocketAddress relayAddress(clientIface.GetAddress(1), clientPort);
    Simulator::Schedule(Seconds(1.0), &Socket::SendTo, clientSocket, Create<Packet>(128), 0, relayAddress);

    // Run the simulation
    Simulator::Stop(Seconds(3.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

