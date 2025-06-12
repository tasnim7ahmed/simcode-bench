#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpRttSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("TcpRttSimulation", LOG_LEVEL_INFO);

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

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 20));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));

    InetSocketAddress inetSinkAddress = InetSocketAddress(interfaces.GetAddress(1), sinkPort);
    Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    clientSocket->TraceConnectWithoutContext("RTT", MakeCallback([](const Time& rtt) {
        NS_LOG_INFO("Measured RTT: " << rtt.As(Time::MS));
    }));

    Simulator::Schedule(Seconds(2.0), [clientSocket, inetSinkAddress]() {
        clientSocket->Connect(inetSinkAddress);
        uint32_t sentBytes = 0;
        uint32_t totalBytes = 1000000;
        Time interval = MilliSeconds(100);

        auto sendPacket = [&sentBytes, totalBytes, interval, clientSocket]() {
            if (sentBytes < totalBytes) {
                Ptr<Packet> packet = Create<Packet>(1000);
                clientSocket->Send(packet);
                sentBytes += 1000;
                Simulator::Schedule(interval, sendPacket);
            } else {
                clientSocket->Close();
            }
        };

        Simulator::ScheduleNow(sendPacket);
    });

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}