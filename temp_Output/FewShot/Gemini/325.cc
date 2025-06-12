#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;
    Address sinkLocalAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    ns3TcpSocket->Bind();
    ns3TcpSocket->Connect(sinkLocalAddress);

    Simulator::Schedule(Seconds(2.0), [&]() {
        ns3TcpSocket->Send(Create<Packet>(1024));
    });

    Time lastRtt;
    ns3TcpSocket->SetRecvCallback([&](Ptr<Socket> socket) {
        Time rtt = socket->GetRttEstimate();
        if (rtt != lastRtt) {
            std::cout << "RTT: " << rtt.GetMilliSeconds() << " ms" << std::endl;
            lastRtt = rtt;
        }
        socket->Recv();
    });

    Simulator::Schedule(Seconds(2.0), [&]() {
        Simulator::Schedule(Seconds(1.0), [&]() {
            ns3TcpSocket->Send(Create<Packet>(1024));
        });
        Simulator::Schedule(Seconds(2.0), [&]() {
            ns3TcpSocket->Send(Create<Packet>(1024));
        });
        Simulator::Schedule(Seconds(3.0), [&]() {
            ns3TcpSocket->Send(Create<Packet>(1024));
        });
        Simulator::Schedule(Seconds(4.0), [&]() {
            ns3TcpSocket->Send(Create<Packet>(1024));
        });
        Simulator::Schedule(Seconds(5.0), [&]() {
            ns3TcpSocket->Send(Create<Packet>(1024));
        });
        Simulator::Schedule(Seconds(6.0), [&]() {
            ns3TcpSocket->Send(Create<Packet>(1024));
        });
        Simulator::Schedule(Seconds(7.0), [&]() {
            ns3TcpSocket->Send(Create<Packet>(1024));
        });
        Simulator::Schedule(Seconds(8.0), [&]() {
            ns3TcpSocket->Send(Create<Packet>(1024));
        });
    });

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}