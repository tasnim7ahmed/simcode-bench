#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpSocketExample");

void SendData(Ptr<Socket> socket, Ipv4Address dstaddr, uint16_t port, std::string data) {
    Ptr<Packet> packet = Create<Packet>((uint8_t*) data.c_str(), data.size());
    socket->SendTo(packet, 0, InetSocketAddress(dstaddr, port));
    NS_LOG_UNCOND("Time " << Simulator::Now().GetSeconds()
                   << "s: Client sent data: \"" << data << "\"");
}

void ReceivePacket(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet;
    while ((packet = socket->RecvFrom(from))) {
        if (packet->GetSize() == 0) {
            break;
        }
        std::string recvData;
        recvData.resize(packet->GetSize());
        packet->CopyData(reinterpret_cast<uint8_t*>(&recvData[0]), packet->GetSize());
        InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
        NS_LOG_UNCOND("Time " << Simulator::Now().GetSeconds()
            << "s: Server received data from " << address.GetIpv4() 
            << ": \"" << recvData << "\"");
    }
}

int main(int argc, char *argv[])
{
    LogComponentEnable("TcpSocketExample", LOG_LEVEL_INFO);

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // 2. Create point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // 3. Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // 4. Assign IP Addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 5. Create TCP socket on server (node 1)
    uint16_t port = 8080;
    Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(1), TcpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
    recvSocket->Bind(local);
    recvSocket->Listen();

    recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // 6. Create TCP socket on client (node 0)
    Ptr<Socket> srcSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

    // 7. Schedule send events on client (after server is listening)
    Simulator::Schedule(Seconds(1.0), &SendData, srcSocket, interfaces.GetAddress(1), port, "Hello, TCP Server!");

    // 8. Run simulation
    Simulator::Stop(Seconds(3.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}