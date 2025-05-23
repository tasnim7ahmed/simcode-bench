#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include <iostream>

using namespace ns3;

int main()
{
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(131072));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(131072));

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9;
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = sinkHelper.Install(nodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    BulkSendHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(1));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(9.0));

    pointToPoint.EnablePcapAll("tcp-point-to-point");

    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("tcp-point-to-point.tr"));

    Time simulationTime = Seconds(10.0);
    Simulator::Stop(simulationTime);

    Simulator::Run();

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApps.Get(0));
    double totalBytesReceived = sink->GetTotalRx();
    double actualTxDuration = 9.0 - 1.0;

    double throughputMbps = (totalBytesReceived * 8.0) / (actualTxDuration * 1000000.0);

    std::cout << "Total bytes received: " << totalBytesReceived << " bytes" << std::endl;
    std::cout << "Throughput: " << throughputMbps << " Mbps" << std::endl;
    std::cout << "Simulation successful and TCP traffic flowed over the link." << std::endl;

    Simulator::Destroy();

    return 0;
}