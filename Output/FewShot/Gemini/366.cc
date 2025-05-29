#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

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

    uint16_t port = 9;
    Address sinkLocalAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    ns3TcpSocket->SetAttribute("CongestionControl", StringValue("ns3::TcpNewReno"));

    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    Ptr<Socket> socket = Socket::CreateSocket(nodes.Get(0), tid);

    socket->Bind();
    socket->Connect(sinkLocalAddress);

    socket->SetRecvCallback([](Ptr<Socket> socket) {
        Address remoteAddress;
        Ptr<Packet> packet = socket->RecvFrom(remoteAddress);
        return packet;
    });

    socket->SetCloseCallback([](Ptr<Socket> socket) {
    });

    socket->SetConnectCallback([](Ptr<Socket> socket) {
    });

    socket->SetAcceptCallback([](Ptr<Socket> socket, const Address& address) {
        return true;
    });

    Time interPacketInterval = Seconds(0.01);
    uint32_t packetSize = 1024;
    Simulator::Schedule(Seconds(2.0), [&, socket, packetSize]() {
        for(int i = 0; i < 100; ++i){
            Ptr<Packet> packet = Create<Packet>(packetSize);
            socket->Send(packet);
            Simulator::Schedule(interPacketInterval, [&, socket, packetSize]() {
                Ptr<Packet> packet = Create<Packet>(packetSize);
                socket->Send(packet);
            });
        }
    });

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}