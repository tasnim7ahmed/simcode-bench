#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/socket.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MinimalTcp");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
    LogComponent::SetDefaultLogFlag(LOG_PREFIX_TIME);

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
    Address sinkLocalAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(1.0));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

    ns3TcpSocket->Connect(sinkLocalAddress);

    Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>("tcp_client.txt", std::ios::out);

    ns3TcpSocket->SetRecvCallback([stream](Ptr<Socket> socket, Ptr<Packet> packet, const Address& from) {
        *stream << Simulator::Now().GetSeconds() << " Received one packet!" << std::endl;
        return true;
    });

    Simulator::ScheduleWithContext(ns3TcpSocket->GetNode()->GetId(), Seconds(0.1), [ns3TcpSocket, stream]() {
        Ptr<Packet> packet = Create<Packet>(1024);
        ns3TcpSocket->Send(packet);
        *stream << Simulator::Now().GetSeconds() << " Sent one packet!" << std::endl;
    });

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}