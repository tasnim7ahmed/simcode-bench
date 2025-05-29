#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

int main(int argc, char* argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(nodes);

    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

    Ptr<Application> clientApp = CreateObject<Application>();
    nodes.Get(0)->AddApplication(clientApp);

    clientApp->SetStartTime(Seconds(2.0));
    clientApp->SetStopTime(Seconds(10.0));

    Time interPacketInterval = Seconds(1.0);
    uint32_t packetSize = 1024;
    uint32_t numPackets = 10;
    
    Simulator::ScheduleWithContext(ns3TcpSocket->GetNode()->GetId(), Seconds(2.0), [ns3TcpSocket, serverAddress, interPacketInterval, packetSize, numPackets]() {
        ns3TcpSocket->Connect(serverAddress);
        ns3TcpSocket->SetRecvCallback([](Ptr<Socket> socket){});
        ns3TcpSocket->SetConnectCallback([](Ptr<Socket> socket){}, [](Ptr<Socket> socket){});

        for (uint32_t i = 0; i < numPackets; ++i) {
            Simulator::Schedule(Seconds(i) * interPacketInterval, [ns3TcpSocket, packetSize]() {
                Ptr<Packet> packet = Create<Packet>(packetSize);
                ns3TcpSocket->Send(packet);
            });
        }
    });

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}