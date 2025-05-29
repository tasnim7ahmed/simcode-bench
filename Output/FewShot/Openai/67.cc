#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;
using namespace std;

void
CwndTrace(std::string filename, Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

void
QueueSizeTrace(std::string filename, Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
}

void experiment(std::string queueDiscType, std::string cwndTraceFile, std::string queueTraceFile, double simTime)
{
    // Parameters
    uint32_t nLeft = 7;      // Number of senders
    uint32_t nRight = 1;     // Number of receivers
    std::string dataRate = "10Mbps";
    std::string bottleneckRate = "2Mbps";
    std::string delay = "2ms";

    // Node containers
    NodeContainer leftNodes;
    leftNodes.Create(nLeft);
    NodeContainer rightNodes;
    rightNodes.Create(nRight);

    NodeContainer routers;
    routers.Create(2); // left router (routers.Get(0)), right router (routers.Get(1))

    // Point-to-Point helpers
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue(dataRate));
    accessLink.SetChannelAttribute("Delay", StringValue(delay));

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckRate));
    bottleneckLink.SetChannelAttribute("Delay", StringValue(delay));

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(routers);

    // NetDevices
    std::vector<NetDeviceContainer> leftAccessDevices;
    std::vector<Ipv4InterfaceContainer> leftAccessIfs;

    Ipv4AddressHelper address;

    // Connect left nodes to left router
    for (uint32_t i = 0; i < nLeft; ++i)
    {
        NetDeviceContainer nd = accessLink.Install(NodeContainer(leftNodes.Get(i), routers.Get(0)));
        leftAccessDevices.push_back(nd);

        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        leftAccessIfs.push_back(address.Assign(nd));
    }

    // Connect right router to single right node
    NetDeviceContainer rightAccessDevices = accessLink.Install(NodeContainer(routers.Get(1), rightNodes.Get(0)));
    address.SetBase("10.2.0.0", "255.255.255.0");
    Ipv4InterfaceContainer rightAccessIfs = address.Assign(rightAccessDevices);

    // Connect left and right routers (bottleneck link)
    NetDeviceContainer bottleneckDevices = bottleneckLink.Install(NodeContainer(routers.Get(0), routers.Get(1)));
    address.SetBase("10.3.0.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckIfs = address.Assign(bottleneckDevices);

    // Assign default routes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // TrafficControl on bottleneck link
    TrafficControlHelper tch;
    uint16_t handle;

    if (queueDiscType == "COBALT")
    {
        tch.SetRootQueueDisc("ns3::CobaltQueueDisc");
    }
    else if (queueDiscType == "CoDel")
    {
        tch.SetRootQueueDisc("ns3::CoDelQueueDisc");
    }
    else
    {
        NS_ABORT_MSG("Unsupported queue disc type");
    }

    QueueDiscContainer qdiscs = tch.Install(bottleneckDevices);

    // TCP traffic: use leftNodes.Get(0) as TCP source
    uint16_t tcpPort = 50001;
    Address tcpSinkAddress(InetSocketAddress(rightAccessIfs.GetAddress(1), tcpPort));
    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", tcpSinkAddress);
    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install(rightNodes.Get(0));
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(Seconds(simTime));

    Ptr<Socket> tcpSocket = Socket::CreateSocket(leftNodes.Get(0), TcpSocketFactory::GetTypeId());
    Ptr<BulkSendApplication> bulkSendApp = CreateObject<BulkSendApplication>();
    bulkSendApp->SetAttribute("Remote", AddressValue(InetSocketAddress(rightAccessIfs.GetAddress(1), tcpPort)));
    bulkSendApp->SetAttribute("MaxBytes", UintegerValue(0));
    leftNodes.Get(0)->AddApplication(bulkSendApp);
    bulkSendApp->SetStartTime(Seconds(0.2));
    bulkSendApp->SetStopTime(Seconds(simTime - 0.01));

    // UDP traffic: from leftNodes.Get(1..6)
    uint16_t udpPort = 40000;
    double udpStartTime = 0.1;
    double udpInterval = 0.015; // 15ms

    for (uint32_t i = 1; i < nLeft; ++i)
    {
        OnOffHelper udpClient("ns3::UdpSocketFactory", InetSocketAddress(rightAccessIfs.GetAddress(1), udpPort + i));
        udpClient.SetAttribute("PacketSize", UintegerValue(1000));
        udpClient.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
        udpClient.SetAttribute("StartTime", TimeValue(Seconds(udpStartTime + i * 0.01)));
        udpClient.SetAttribute("StopTime", TimeValue(Seconds(simTime - 0.01)));
        udpClient.Install(leftNodes.Get(i));

        PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory",
                                       InetSocketAddress(Ipv4Address::GetAny(), udpPort + i));
        ApplicationContainer sinkApp = udpSinkHelper.Install(rightNodes.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simTime));
    }

    // Cwnd Trace for TCP (leftNodes.Get(0))
    Ptr<OutputStreamWrapper> cwndStream = Create<OutputStreamWrapper>(cwndTraceFile, std::ios::out);
    leftNodes.Get(0)->GetObject<TcpL4Protocol> ();
    Ptr<Socket> tcpTraceSocket = tcpSocket;
    tcpTraceSocket->TraceConnectWithoutContext(
        "CongestionWindow",
        MakeBoundCallback(&CwndTrace, cwndTraceFile, cwndStream));

    // Queue size trace for bottleneck qdisc (trace the first qdisc)
    Ptr<QueueDisc> qdisc = qdiscs.Get(0);
    Ptr<OutputStreamWrapper> queueStream = Create<OutputStreamWrapper>(queueTraceFile, std::ios::out);

    qdisc->TraceConnectWithoutContext(
        "PacketsInQueue",
        MakeBoundCallback(&QueueSizeTrace, queueTraceFile, queueStream));

    // Clean animation file
    //AnimationInterface::SetConstantPosition(leftNodes.Get(0), 10.0, 30.0); // can comment-out animation

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    double simTime = 10.0; // seconds

    experiment("COBALT", "cwnd-cobalt.txt", "queue-cobalt.txt", simTime);
    experiment("CoDel", "cwnd-codel.txt", "queue-codel.txt", simTime);

    return 0;
}