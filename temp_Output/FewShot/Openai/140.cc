#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpSocketExample");

// Server-side callback for incoming connections
void
AcceptCallback(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_INFO("Server: Accepted connection from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
}

// Client-side callback for send events
void
SendData(Ptr<Socket> socket, uint32_t toSend)
{
    static uint32_t sent = 0;
    while (socket->GetTxAvailable() && sent < toSend)
    {
        uint32_t left = toSend - sent;
        uint32_t dataSize = std::min(left, 1024u);
        Ptr<Packet> packet = Create<Packet>(dataSize);
        int actual = socket->Send(packet);
        if (actual > 0)
        {
            NS_LOG_INFO("Client: Sent " << actual << " bytes at " << Simulator::Now().GetSeconds() << "s");
            sent += actual;
        }
        if ((uint32_t)actual < dataSize)
        {
            // waiting for send buffer to clear
            socket->SetSendCallback(MakeCallback(&SendData));
            break;
        }
    }
}

// Server-side data reception callback
void
ReceiveCallback(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        if (packet->GetSize() > 0)
        {
            NS_LOG_INFO("Server: Received " << packet->GetSize() << " bytes at " << Simulator::Now().GetSeconds() << "s");
        }
    }
}

int main(int argc, char *argv[])
{
    LogComponentEnable("TcpSocketExample", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up point-to-point link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // TCP server socket on node 1
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(1), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 8080);
    serverSocket->Bind(local);
    serverSocket->Listen();
    serverSocket->SetAcceptCallback(
        MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
        MakeCallback(&AcceptCallback)
    );
    serverSocket->SetRecvCallback(MakeCallback(&ReceiveCallback));

    // TCP client socket on node 0
    Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), tid);

    // Client-side: connect to server and send data
    Simulator::Schedule(Seconds(1.0), [=] {
        InetSocketAddress remote(interfaces.GetAddress(1), 8080);
        clientSocket->Connect(remote);
        uint32_t totalSize = 4096; // bytes to send (4 packets of 1024 bytes)
        Simulator::Schedule(Seconds(1.1), [clientSocket, totalSize]() {
            SendData(clientSocket, totalSize);
        });
    });

    // Simulation end
    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}