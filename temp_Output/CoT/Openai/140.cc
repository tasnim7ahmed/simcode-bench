#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpSocketExample");

void ReceivePacket(Ptr<Socket> socket)
{
    while (Ptr<Packet> packet = socket->Recv())
    {
        NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s Server received "
            << packet->GetSize() << " bytes");
    }
}

void SendPacket(Ptr<Socket> socket, Address peerAddress, uint32_t pktSize, uint32_t nPkts, Time pktInterval)
{
    static uint32_t sent = 0;
    Ptr<Packet> packet = Create<Packet>(pktSize);

    int result = socket->SendTo(packet, 0, peerAddress);
    if (result >= 0)
    {
        NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s Client sent "
            << pktSize << " bytes (Packet #" << sent+1 << ")");
    }

    sent++;
    if (sent < nPkts)
    {
        Simulator::Schedule(pktInterval, &SendPacket, socket, peerAddress, pktSize, nPkts, pktInterval);
    }
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("TcpSocketExample", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;

    // SERVER: Node 1
    TypeId tcpTypeId = TypeId::LookupByName("ns3::TcpSocketFactory");
    Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(1), tcpTypeId);

    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
    recvSocket->Bind(local);
    recvSocket->Listen();

    recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // CLIENT: Node 0
    Simulator::Schedule(Seconds(1.0), [=]() {
        Ptr<Socket> sendSocket = Socket::CreateSocket(nodes.Get(0), tcpTypeId);
        InetSocketAddress remote = InetSocketAddress(interfaces.GetAddress(1), port);
        sendSocket->Connect(remote);

        // Send 5 packets of 1024 bytes each at 50ms interval
        SendPacket(sendSocket, remote, 1024, 5, MilliSeconds(50));
    });

    Simulator::Stop(Seconds(3.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}