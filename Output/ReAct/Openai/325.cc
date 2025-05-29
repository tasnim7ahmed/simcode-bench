#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/tcp-socket.h"
#include "ns3/tcp-congestion-ops.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpRttExample");

void
LogRtt(Ptr<Socket> socket)
{
    Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase>(socket);
    if (tcpSocket)
    {
        Time rtt = tcpSocket->GetRtt();
        double rttMs = rtt.GetMilliSeconds();
        Simulator::Now().Print(std::cout);
        std::cout << " RTT = " << rttMs << " ms" << std::endl;
    }
    Simulator::Schedule(Seconds(0.1), &LogRtt, socket); // Log every 100ms
}

int
main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;

    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApps = packetSinkHelper.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

    // Set up and install BulkSendApplication as client
    BulkSendHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited

    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Connect log RTT
    Simulator::Schedule(Seconds(2.0), &LogRtt, clientSocket);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}