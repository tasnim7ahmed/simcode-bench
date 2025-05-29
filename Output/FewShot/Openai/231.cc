#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void
RxCallback(Ptr<const Packet> packet, const Address &address)
{
    NS_LOG_UNCOND("Server received packet of size " << packet->GetSize() << " bytes.");
}

int
main(int argc, char *argv[])
{
    LogComponentEnable("TcpL4Protocol", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;

    // Server socket
    Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(1), TcpSocketFactory::GetTypeId());
    InetSocketAddress localAddress = InetSocketAddress(Ipv4Address::GetAny(), port);
    recvSocket->Bind(localAddress);
    recvSocket->Listen();
    recvSocket->SetRecvCallback(MakeCallback(&RxCallback));

    // Client application: will connect and send data
    Simulator::Schedule(Seconds(1.0), [=]() {
        Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
        InetSocketAddress serverAddress = InetSocketAddress(interfaces.GetAddress(1), port);
        clientSocket->Connect(serverAddress);

        Ptr<Packet> packet = Create<Packet>(128);
        clientSocket->Send(packet);

        clientSocket->Close();
    });

    Simulator::Stop(Seconds(2.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}