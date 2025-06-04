#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpExample");

void ReceivePacket(Ptr<Socket> socket)
{
    while (socket->Recv())
    {
        NS_LOG_UNCOND("Received one packet!");
    }
}

int main(int argc, char *argv[])
{
    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Set up mobility model (static positions)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set up TCP socket
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    Ptr<Socket> tcpServerSocket = Socket::CreateSocket(nodes.Get(1), tid);

    // Bind the server socket to a port
    InetSocketAddress serverAddress = InetSocketAddress(Ipv4Address::GetAny(), 8080);
    tcpServerSocket->Bind(serverAddress);
    tcpServerSocket->Listen();
    tcpServerSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Assign IP addresses to the nodes
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(nodes);

    // Set up TCP client on Node 0
    Ptr<Socket> tcpClientSocket = Socket::CreateSocket(nodes.Get(0), tid);
    InetSocketAddress clientAddress = InetSocketAddress(interfaces.GetAddress(1), 8080);
    tcpClientSocket->Connect(clientAddress);

    // Create a TCP application (client)
    Ptr<Socket> appSocket = Socket::CreateSocket(nodes.Get(0), tid);
    Ptr<TcpClient> clientApp = CreateObject<TcpClient>();
    clientApp->Setup(appSocket, InetSocketAddress(interfaces.GetAddress(1), 8080), 1000);
    nodes.Get(0)->AddApplication(clientApp);
    clientApp->SetStartTime(Seconds(1.0));
    clientApp->SetStopTime(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
