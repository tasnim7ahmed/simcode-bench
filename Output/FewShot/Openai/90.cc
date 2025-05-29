#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/error-model.h"
#include <fstream>

using namespace ns3;

static std::ofstream traceFile;

void
RxTrace(std::string context, Ptr<const Packet> packet, const Address &address)
{
    traceFile << Simulator::Now().GetSeconds() << " " << context
              << " PacketUid=" << packet->GetUid()
              << " Rx" << std::endl;
}

void
QueueTrace(std::string context, Ptr<const QueueDiscItem> item)
{
    if (item)
    {
        traceFile << Simulator::Now().GetSeconds() << " " << context
                  << " PacketUid=" << item->GetPacket()->GetUid()
                  << " Enqueued" << std::endl;
    }
}

void
DequeueTrace(std::string context, Ptr<const QueueDiscItem> item)
{
    if (item)
    {
        traceFile << Simulator::Now().GetSeconds() << " " << context
                  << " PacketUid=" << item->GetPacket()->GetUid()
                  << " Dequeued" << std::endl;
    }
}

void
DropTrace(std::string context, Ptr<const QueueDiscItem> item)
{
    if (item)
    {
        traceFile << Simulator::Now().GetSeconds() << " " << context
                  << " PacketUid=" << item->GetPacket()->GetUid()
                  << " Dropped" << std::endl;
    }
}

int
main(int argc, char *argv[])
{
    // Open trace file
    traceFile.open("simple-error-model.tr");
    if (!traceFile.is_open())
    {
        NS_FATAL_ERROR("Cannot open trace file.");
    }

    // Create nodes
    NodeContainer n0n2, n1n2, n2n3;
    n0n2.Add(CreateObject<Node>());
    n0n2.Add(CreateObject<Node>()); // n2
    Ptr<Node> n0 = n0n2.Get(0);
    Ptr<Node> n2 = n0n2.Get(1);

    Ptr<Node> n1 = CreateObject<Node>();
    NodeContainer n1n2_tmp(n1, n2);
    n1n2 = n1n2_tmp;

    Ptr<Node> n3 = CreateObject<Node>();
    NodeContainer n2n3_tmp(n2, n3);
    n2n3 = n2n3_tmp;

    // All nodes in an ordered container for reference
    NodeContainer allNodes(n0, n1, n2, n3);

    // Configure point-to-point links
    PointToPointHelper p2p_n0n2;
    p2p_n0n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_n0n2.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p_n1n2;
    p2p_n1n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_n1n2.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p_n2n3;
    p2p_n2n3.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_n2n3.SetChannelAttribute("Delay", StringValue("10ms"));

    // DropTail queues are default

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Install devices & channels
    NetDeviceContainer d_n0n2 = p2p_n0n2.Install(n0, n2);
    NetDeviceContainer d_n1n2 = p2p_n1n2.Install(n1, n2);
    NetDeviceContainer d_n2n3 = p2p_n2n3.Install(n2, n3);

    // Set error models
    Ptr<RateErrorModel> rateError = CreateObject<RateErrorModel>();
    rateError->SetAttribute("ErrorRate", DoubleValue(0.001));
    rateError->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));

    Ptr<BurstErrorModel> burstError = CreateObject<BurstErrorModel>();
    burstError->SetAttribute("ErrorRate", DoubleValue(0.01));
    burstError->SetAttribute("BurstSize", UintegerValue(2));

    Ptr<ListErrorModel> listError = CreateObject<ListErrorModel>();
    std::list<uint32_t> uids;
    uids.push_back(11);
    uids.push_back(17);
    listError->SetList(uids);

    // Attach error models to receive side of link
    d_n2n3.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(rateError));
    d_n2n3.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(burstError));
    d_n0n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(listError));

    // Assign IP addresses
    Ipv4AddressHelper addr_n0n2, addr_n1n2, addr_n2n3;
    addr_n0n2.SetBase("10.1.1.0", "255.255.255.0");
    addr_n1n2.SetBase("10.1.2.0", "255.255.255.0");
    addr_n2n3.SetBase("10.1.3.0", "255.255.255.0");

    Ipv4InterfaceContainer if_n0n2 = addr_n0n2.Assign(d_n0n2); // n0: .1, n2: .2
    Ipv4InterfaceContainer if_n1n2 = addr_n1n2.Assign(d_n1n2); // n1: .1, n2: .2
    Ipv4InterfaceContainer if_n2n3 = addr_n2n3.Assign(d_n2n3); // n2: .1, n3: .2

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install queue and tracing for DropTail
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::DropTailQueueDisc", "MaxSize", StringValue("100p"));
    QueueDiscContainer qd_n0n2 = tch.Install(d_n0n2);
    QueueDiscContainer qd_n1n2 = tch.Install(d_n1n2);
    QueueDiscContainer qd_n2n3 = tch.Install(d_n2n3);

    qd_n0n2.Get(0)->TraceConnect("Enqueue", "N0N2", MakeBoundCallback(&QueueTrace, "N0N2"));
    qd_n0n2.Get(0)->TraceConnect("Dequeue", "N0N2", MakeBoundCallback(&DequeueTrace, "N0N2"));
    qd_n0n2.Get(0)->TraceConnect("Drop", "N0N2", MakeBoundCallback(&DropTrace, "N0N2"));
    qd_n1n2.Get(0)->TraceConnect("Enqueue", "N1N2", MakeBoundCallback(&QueueTrace, "N1N2"));
    qd_n1n2.Get(0)->TraceConnect("Dequeue", "N1N2", MakeBoundCallback(&DequeueTrace, "N1N2"));
    qd_n1n2.Get(0)->TraceConnect("Drop", "N1N2", MakeBoundCallback(&DropTrace, "N1N2"));
    qd_n2n3.Get(0)->TraceConnect("Enqueue", "N2N3", MakeBoundCallback(&QueueTrace, "N2N3"));
    qd_n2n3.Get(0)->TraceConnect("Dequeue", "N2N3", MakeBoundCallback(&DequeueTrace, "N2N3"));
    qd_n2n3.Get(0)->TraceConnect("Drop", "N2N3", MakeBoundCallback(&DropTrace, "N2N3"));

    // UDP CBR flow: n0->n3
    uint16_t udpPort1 = 9001;
    UdpServerHelper udpServer1(udpPort1);
    ApplicationContainer apps_server1 = udpServer1.Install(n3);
    apps_server1.Start(Seconds(0.0));
    apps_server1.Stop(Seconds(10.0));

    UdpClientHelper udpClient1(if_n2n3.GetAddress(1), udpPort1);
    udpClient1.SetAttribute("PacketSize", UintegerValue(210));
    udpClient1.SetAttribute("MaxPackets", UintegerValue(1000000));
    udpClient1.SetAttribute("Interval", TimeValue(Seconds(0.00375)));

    ApplicationContainer apps_client1 = udpClient1.Install(n0);
    apps_client1.Start(Seconds(0.1));
    apps_client1.Stop(Seconds(10.0));

    // UDP CBR flow: n3->n1
    uint16_t udpPort2 = 9002;
    UdpServerHelper udpServer2(udpPort2);
    ApplicationContainer apps_server2 = udpServer2.Install(n1);
    apps_server2.Start(Seconds(0.0));
    apps_server2.Stop(Seconds(10.0));

    UdpClientHelper udpClient2(if_n1n2.GetAddress(0), udpPort2);
    udpClient2.SetAttribute("PacketSize", UintegerValue(210));
    udpClient2.SetAttribute("MaxPackets", UintegerValue(1000000));
    udpClient2.SetAttribute("Interval", TimeValue(Seconds(0.00375)));

    ApplicationContainer apps_client2 = udpClient2.Install(n3);
    apps_client2.Start(Seconds(0.1));
    apps_client2.Stop(Seconds(10.0));

    // Trace UDP receptions
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&RxTrace));
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpServer/RxWithAddresses", MakeCallback(&RxTrace));

    // FTP/TCP flow: n0 -> n3
    uint16_t ftpPort = 21;
    Address ftpAddr(InetSocketAddress(if_n2n3.GetAddress(1), ftpPort));

    BulkSendHelper ftpSource("ns3::TcpSocketFactory", ftpAddr);
    ftpSource.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer ftpApp = ftpSource.Install(n0);
    ftpApp.Start(Seconds(1.2));
    ftpApp.Stop(Seconds(1.35));

    PacketSinkHelper ftpSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), ftpPort));
    ApplicationContainer ftpSinkApp = ftpSink.Install(n3);
    ftpSinkApp.Start(Seconds(1.19));
    ftpSinkApp.Stop(Seconds(10.0));

    // Trace TCP receptions
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&RxTrace));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    traceFile.close();

    Simulator::Destroy();
    return 0;
}