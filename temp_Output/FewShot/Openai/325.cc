#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void
RttTracer(Ptr<OutputStreamWrapper> stream, Time oldRtt, Time newRtt)
{
    *stream->GetStream() << Simulator::Now().GetSeconds()
                        << "s RTT: " << newRtt.GetMilliSeconds() << " ms" << std::endl;
}

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Point-to-point helper
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // IP addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // TCP server setup (PacketSink)
    uint16_t port = 50000;
    Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", localAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // TCP client setup (BulkSendApplication)
    BulkSendHelper bulkSender("ns3::TcpSocketFactory",
                              InetSocketAddress(interfaces.GetAddress(1), port));
    bulkSender.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited
    ApplicationContainer clientApp = bulkSender.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // RTT logging
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    Ptr<RttEstimator> rtt = ns3TcpSocket->GetObject<TcpSocketBase>()->GetRtt();
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("rtt-trace.log");

    // Attach RTT trace
    ns3TcpSocket->TraceConnectWithoutContext("RTT", MakeBoundCallback(&RttTracer, stream));

    bulkSender.SetAttribute("SendSize", UintegerValue(1024));

    // Instead of using a random socket, instruct BulkSend to use our socket (for RTT tracing)
    clientApp.Get(0)->GetObject<BulkSendApplication>()->SetSocket(ns3TcpSocket);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}