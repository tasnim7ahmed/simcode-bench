#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(536));
    Config::SetDefault("ns3::TcpSocketBase::MaxWindowSize", UintegerValue(65535));

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p01;
    p2p01.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p01.SetChannelAttribute("Delay", StringValue("1ms"));

    PointToPointHelper p2p12;
    p2p12.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p12.SetChannelAttribute("Delay", StringValue("10ms"));

    PointToPointHelper p2p23;
    p2p23.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p23.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer d0d1 = p2p01.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d1d2 = p2p12.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d2d3 = p2p23.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.NewNetwork();
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

    address.NewNetwork();
    Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(i2i3.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(3));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApps = bulkSend.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(9.0));

    AsciiTraceHelper asciiTraceHelper;
    std::ofstream cwndStream;
    cwndStream.open("results/cwndTraces.tr");
    Config::Connect("/NodeList/0/ApplicationList/0/$ns3::BulkSendApplication/Socket/Tx", MakeBoundCallback(&CwndChange, &cwndStream));
    Config::Connect("/NodeList/3/ApplicationList/0/$ns3::PacketSink/Rx", MakeBoundCallback(&Rx, &cwndStream));

    TrafficControlHelper tc;
    tc.SetRootQueueDisc("ns3::PfifoFastQueueDisc", "Limit", UintegerValue(1000));
    QueueDiscContainer qdiscs;
    qdiscs = tc.Install(d1d2.Get(0));
    Ptr<QueueDisc> q = qdiscs.Get(0);
    std::ofstream queueSizeFile("results/queue-size.dat");
    q->TraceConnectWithoutContext("Enqueue", MakeBoundCallback(&QueueSize, &queueSizeFile));
    std::ofstream queueDropFile("results/queueTraces.tr");
    q->TraceConnectWithoutContext("Drop", MakeBoundCallback(&QueueDrop, &queueDropFile));
    std::ofstream queueStatsFile("results/queueStats.txt");
    q->CollectStatistics(Seconds(0.1));
    Simulator::Schedule(Seconds(0.1), &PrintQueueStats, q, &queueStatsFile);

    p2p01.EnablePcapAll("results/pcap/n0n1");
    p2p12.EnablePcapAll("results/pcap/n1n2");
    p2p23.EnablePcapAll("results/pcap/n2n3");

    std::ofstream configOut("results/config.txt");
    configOut << "Link n0-n1: 10 Mbps, 1 ms delay\n";
    configOut << "Link n1-n2: 1 Mbps, 10 ms delay\n";
    configOut << "Link n2-n3: 10 Mbps, 1 ms delay\n";
    configOut << "TCP Flow: n0 to n3 using BulkSendApplication\n";
    configOut << "Simulation Duration: 10 seconds\n";
    configOut << "Output directories: cwndTraces, pcap, queueTraces, queue-size.dat, queueStats.txt\n";

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}