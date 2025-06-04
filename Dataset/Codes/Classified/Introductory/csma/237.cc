#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpRelayExample");

void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("Server received a UDP packet!");
    }
}

int main() {
    LogComponentEnable("CsmaUdpRelayExample", LOG_LEVEL_INFO);

    uint16_t port = 4000;

    // Create nodes (client, relay, server)
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> client = nodes.Get(0);
    Ptr<Node> relay = nodes.Get(1);
    Ptr<Node> server = nodes.Get(2);

    // Setup CSMA network
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = csma.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP Server (Server Node)
    Ptr<Socket> serverSocket = Socket::CreateSocket(server, UdpSocketFactory::GetTypeId());
    serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
    serverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Client (Client Node)
    Ptr<Socket> clientSocket = Socket::CreateSocket(client, UdpSocketFactory::GetTypeId());
    InetSocketAddress serverAddress(interfaces.GetAddress(2), port);

    // Send packets at scheduled times
    Simulator::Schedule(Seconds(1.0), &Socket::SendTo, clientSocket, Create<Packet>(128), 0, serverAddress);
    Simulator::Schedule(Seconds(2.0), &Socket::SendTo, clientSocket, Create<Packet>(128), 0, serverAddress);

    // Run the simulation
    Simulator::Stop(Seconds(3.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
