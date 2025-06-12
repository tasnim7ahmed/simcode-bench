#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <string>

using namespace ns3;

void
CwndChangeCallback(std::string filename, uint32_t flow, uint32_t oldCwnd, uint32_t newCwnd)
{
    static std::map<std::string, std::ofstream> files;
    if (files.find(filename) == files.end())
    {
        files[filename].open(filename, std::ios::out | std::ios::trunc);
        files[filename] << "Time(s)\tCwnd(Bytes)" << std::endl;
    }
    files[filename] << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

void
QueueDiscSizeTrace (Ptr<QueueDisc> queue, std::string filename)
{
    static std::map<std::string, std::ofstream> files;
    if (files.find(filename) == files.end())
    {
        files[filename].open(filename, std::ios::out | std::ios::trunc);
        files[filename] << "Time(s)\tQueueSize(Bytes)" << std::endl;
    }
    files[filename] << Simulator::Now().GetSeconds() << "\t" << queue->GetNBytes() << std::endl;
}

void
QueueSizeTracer(Ptr<QueueDisc> queue, std::string filename)
{
    QueueDiscSizeTrace(queue, filename);
    Simulator::Schedule(MilliSeconds(10), &QueueSizeTracer, queue, filename);
}

void
experiment(std::string queueType, std::string cwndTraceFile, std::string queueTraceFile)
{
    uint32_t nLeft = 7;
    uint32_t nRight = 1;
    bool tracing = false;
    double simTime = 10.0;

    // Create nodes
    NodeContainer leftNodes, rightNodes, routers;
    leftNodes.Create(nLeft);
    rightNodes.Create(nRight);
    routers.Create(2); // router 0 (left), router 1 (right)

    // Left side links
    PointToPointHelper pointToPointLeaf;
    pointToPointLeaf.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPointLeaf.SetChannelAttribute("Delay", StringValue("1ms"));

    // Backbone link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("30ms"));

    // Install protocol stack
    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(routers);

    // Connect left nodes to router 0
    std::vector<NetDeviceContainer> leftDevices;
    std::vector<Ipv4InterfaceContainer> leftIntfs;
    Ipv4AddressHelper address;
    for (uint32_t i = 0; i < nLeft; ++i)
    {
        NodeContainer pair(leftNodes.Get(i), routers.Get(0));
        NetDeviceContainer devices = pointToPointLeaf.Install(pair);
        leftDevices.push_back(devices);
        std::ostringstream subnet;
        subnet << "10.1." << (i+1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        leftIntfs.push_back(address.Assign(devices));
    }

    // Connect router 1 to right node
    NetDeviceContainer rightDevices = pointToPointLeaf.Install(NodeContainer(routers.Get(1), rightNodes.Get(0)));
    address.SetBase("10.2.0.0", "255.255.255.0");
    Ipv4InterfaceContainer rightIntfs = address.Assign(rightDevices);

    // Connect router 0 and router 1 (bottleneck)
    NetDeviceContainer coreDevices = p2p.Install(NodeContainer(routers.Get(0), routers.Get(1)));
    address.SetBase("10.3.0.0", "255.255.255.0");
    Ipv4InterfaceContainer coreIntfs = address.Assign(coreDevices);

    // Setup queue disc on bottleneck (router 0, outgoing to router 1)
    TrafficControlHelper tch;
    if (queueType == "cobalt")
        tch.SetRootQueueDisc("ns3::CobaltQueueDisc");
    else
        tch.SetRootQueueDisc("ns3::CoDelQueueDisc");
    QueueDiscContainer queueDiscs = tch.Install(coreDevices.Get(0));

    // Routers: enable IP forwarding
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install TCP traffic: flows 0,1,2 (senders 0,1,2)
    uint16_t tcpPort = 50000;
    Address tcpSinkAddr (InetSocketAddress(rightIntfs.GetAddress(1), tcpPort));
    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", tcpSinkAddr);
    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install(rightNodes.Get(0));
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(Seconds(simTime));

    for (uint32_t i = 0; i < 3; ++i)
    {
        Ptr<Socket> tcpSocket = Socket::CreateSocket(leftNodes.Get(i), TcpSocketFactory::GetTypeId());
        Ptr<BulkSendApplication> tcpSource = CreateObject<BulkSendApplication>();
        tcpSource->SetAttribute("Remote", AddressValue(InetSocketAddress(rightIntfs.GetAddress(1), tcpPort)));
        tcpSource->SetAttribute("MaxBytes", UintegerValue(0));
        leftNodes.Get(i)->AddApplication(tcpSource);
        tcpSource->SetStartTime(Seconds(1.0 + i * 0.2));
        tcpSource->SetStopTime(Seconds(simTime));

        std::ostringstream traceFile;
        traceFile << cwndTraceFile.substr(0, cwndTraceFile.size()-4) << "_flow" << i << ".txt";
        tcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChangeCallback, traceFile.str(), i));
        tcpSource->SetSocket(tcpSocket);

        if (i == 0)
        {
            // Trace first flow's congestion window in main cwndTraceFile as well
            tcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChangeCallback, cwndTraceFile, i));
        }
    }

    // Install UDP traffic: flows 3-6 (senders 3-6)
    uint16_t udpPort = 40000;
    Address udpSinkAddr (InetSocketAddress(rightIntfs.GetAddress(1), udpPort));
    PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory", udpSinkAddr);
    ApplicationContainer udpSinkApp = udpSinkHelper.Install(rightNodes.Get(0));
    udpSinkApp.Start(Seconds(0.0));
    udpSinkApp.Stop(Seconds(simTime));

    for (uint32_t i = 3; i < nLeft; ++i)
    {
        OnOffHelper udpSource("ns3::UdpSocketFactory", udpSinkAddr);
        udpSource.SetAttribute("DataRate", DataRateValue(DataRate("6Mbps")));
        udpSource.SetAttribute("PacketSize", UintegerValue(1024));
        udpSource.SetAttribute("StartTime", TimeValue(Seconds(1.5 + (i - 3) * 0.2)));
        udpSource.SetAttribute("StopTime", TimeValue(Seconds(simTime)));
        ApplicationContainer udpApp = udpSource.Install(leftNodes.Get(i));
    }

    // Trace queue disc size
    if (queueDiscs.GetN() > 0)
    {
        Ptr<QueueDisc> qd = queueDiscs.Get(0);
        Simulator::Schedule(Seconds(0.0), &QueueSizeTracer, qd, queueTraceFile);
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
}

int
main(int argc, char *argv[])
{
    experiment("cobalt", "tcp_cwnd_cobalt.txt", "queue_cobalt.txt");
    experiment("codel", "tcp_cwnd_codel.txt", "queue_codel.txt");
    return 0;
}