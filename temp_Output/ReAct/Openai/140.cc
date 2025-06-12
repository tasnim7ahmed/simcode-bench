#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpSocketExample");

void
SendData(Ptr<Socket> sock, Ipv4Address dstaddr, uint16_t port, uint32_t pktSize, uint32_t numPkts, Time pktInterval)
{
    static uint32_t packetsSent = 0;
    Ptr<Packet> pkt = Create<Packet>(pktSize);
    int retval = sock->SendTo(pkt, 0, InetSocketAddress(dstaddr, port));
    if(retval >= 0)
    {
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Sent packet " << packetsSent + 1);
        ++packetsSent;
    }
    if(packetsSent < numPkts)
    {
        Simulator::Schedule(pktInterval, &SendData, sock, dstaddr, port, pktSize, numPkts, pktInterval);
    }
}

void
RecvPacket(Ptr<Socket> socket)
{
    while (Ptr<Packet> pkt = socket->Recv())
    {
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Received packet of " << pkt->GetSize() << " bytes");
    }
}

int
main(int argc, char *argv[])
{
    LogComponentEnable("TcpSocketExample", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces = ipv4.Assign(devices);

    uint16_t servPort = 50000;
    uint32_t pktSize = 512;
    uint32_t numPkts = 10;
    Time pktInterval = MilliSeconds(100);

    // Create server socket on node 1
    Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(1), TcpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), servPort);
    recvSocket->Bind(local);
    recvSocket->Listen();
    recvSocket->SetRecvCallback(MakeCallback(&RecvPacket));

    // Allow time for the server to be up
    Simulator::Schedule(Seconds(1.0), [=]() {
        // Create client socket on node 0
        Ptr<Socket> sendSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
        sendSocket->Bind();
        InetSocketAddress remoteAddr = InetSocketAddress(ifaces.GetAddress(1), servPort);
        sendSocket->Connect(remoteAddr);

        // Start sending packets
        SendData(sendSocket, ifaces.GetAddress(1), servPort, pktSize, numPkts, pktInterval);
    });

    Simulator::Stop(Seconds(3.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}