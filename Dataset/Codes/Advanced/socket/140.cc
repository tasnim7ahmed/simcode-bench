#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpSocketExample");

// Callback function to log when a packet is received at the server
void ReceivePacket(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    while ((packet = socket->Recv()))
    {
        NS_LOG_INFO("Received packet of size " << packet->GetSize());
    }
}

// Function to simulate a client sending data via a TCP socket
void SendPacket(Ptr<Socket> socket, uint32_t size, Time sendInterval)
{
    Ptr<Packet> packet = Create<Packet>(size); // Create a packet of given size
    socket->Send(packet); // Send the packet
    NS_LOG_INFO("Sent packet of size " << size);
    Simulator::Schedule(sendInterval, &SendPacket, socket, size, sendInterval); // Schedule the next send
}

int 
main(int argc, char *argv[])
{
    // Log settings
    LogComponentEnable("TcpSocketExample", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2); // Two nodes: n0 (client) -> n1 (server)

    // Set up a point-to-point link between the two nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the devices on the nodes
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the Internet stack (TCP/IP) on both nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create a TCP socket on the server (node 1)
    Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(1), TcpSocketFactory::GetTypeId());
    InetSocketAddress localAddress = InetSocketAddress(Ipv4Address::GetAny(), 8080); // Listening on port 8080
    serverSocket->Bind(localAddress); // Bind the socket to the address
    serverSocket->Listen(); // Set the socket to listen mode
    serverSocket->SetRecvCallback(MakeCallback(&ReceivePacket)); // Set callback to handle received packets

    // Create a TCP socket on the client (node 0)
    Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    InetSocketAddress remoteAddress = InetSocketAddress(interfaces.GetAddress(1), 8080); // Server address (node 1)
    clientSocket->Connect(remoteAddress); // Connect the client socket to the server

    // Schedule the first packet transmission at 2 seconds
    Simulator::Schedule(Seconds(2.0), &SendPacket, clientSocket, 1024, Seconds(1.0)); // 1024 bytes, every 1 second

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

