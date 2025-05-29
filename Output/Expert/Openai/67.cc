#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

void CwndTracer(std::string filename, uint32_t oldCwnd, uint32_t newCwnd)
{
    static std::ofstream out (filename, std::ios::out);
    out << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

void QueueDiscTracer(std::string filename, Ptr<QueueDisc> q)
{
    static std::ofstream out (filename, std::ios::out);
    out << Simulator::Now().GetSeconds() << "\t" << q->GetQueueSize().GetValue() << std::endl;
    Simulator::Schedule(Seconds(0.01), &QueueDiscTracer, filename, q);
}

void experiment(std::string queueDiscType)
{
    uint32_t nLeft = 7, nRight = 1;
    double simTime = 20.0;

    NodeContainer leftNodes, rightNodes, routers;
    leftNodes.Create(nLeft);
    rightNodes.Create(nRight);
    routers.Create(2); // Left router, right router

    // Point-to-Point settings
    PointToPointHelper p2pLeaf, p2pRouter;
    p2pLeaf.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pLeaf.SetChannelAttribute("Delay", StringValue("2ms"));

    p2pRouter.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pRouter.SetChannelAttribute("Delay", StringValue("20ms"));

    // Connecting left nodes to left router
    NetDeviceContainer leftDevices, routerLeftDevices;
    for (uint32_t i = 0; i < nLeft; ++i)
    {
        NodeContainer pair(leftNodes.Get(i), routers.Get(0));
        NetDeviceContainer linkDevs = p2pLeaf.Install(pair);
        leftDevices.Add(linkDevs.Get(0));
        routerLeftDevices.Add(linkDevs.Get(1));
    }
    // Connecting right node to right router
    NetDeviceContainer rightDevices, routerRightDevices;
    for (uint32_t i = 0; i < nRight; ++i)
    {
        NodeContainer pair(rightNodes.Get(i), routers.Get(1));
        NetDeviceContainer linkDevs = p2pLeaf.Install(pair);
        rightDevices.Add(linkDevs.Get(0));
        routerRightDevices.Add(linkDevs.Get(1));
    }
    // Connecting two routers (bottleneck)
    NetDeviceContainer routerRouterDevices = p2pRouter.Install(routers);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(leftNodes);
    internet.Install(rightNodes);
    internet.Install(routers);

    // Address assignment
    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> leftIfs, rightIfs;
    for (uint32_t i = 0; i < nLeft; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        ipv4.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        Ipv4InterfaceContainer iface = ipv4.Assign(NetDeviceContainer(leftDevices.Get(i), routerLeftDevices.Get(i)));
        leftIfs.push_back(iface);
    }
    for (uint32_t i = 0; i < nRight; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.2." << i << ".0";
        ipv4.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        Ipv4InterfaceContainer iface = ipv4.Assign(NetDeviceContainer(rightDevices.Get(i), routerRightDevices.Get(i)));
        rightIfs.push_back(iface);
    }
    ipv4.SetBase("10.3.1.0", "255.255.255.0");
    Ipv4InterfaceContainer routerIf = ipv4.Assign(routerRouterDevices);

    // Set up queue disc on router's bottleneck devices
    TrafficControlHelper tch;
    if (queueDiscType == "cobalt")
        tch.SetRootQueueDisc("ns3::CobaltQueueDisc");
    else
        tch.SetRootQueueDisc("ns3::CoDelQueueDisc");

    Ptr<NetDevice> routerLeft = routerRouterDevices.Get(0);
    Ptr<NetDevice> routerRight = routerRouterDevices.Get(1);
    QueueDiscContainer qDiscs = tch.Install(routerLeft);

    // Set up TCP and UDP sources
    uint16_t tcpPort = 50000;
    uint16_t udpPort = 40000;
    ApplicationContainer tcpApps, udpApps;

    // Install sink on right-most node
    Address tcpSinkAddress(InetSocketAddress(rightIfs[0].GetAddress(0), tcpPort));
    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", tcpSinkAddress);
    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install(rightNodes.Get(0));
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(Seconds(simTime + 1));

    // TCP sources: sender 0-5
    for (uint32_t i = 0; i < nLeft-1; ++i)
    {
        Ptr<Socket> tcpSocket = Socket::CreateSocket(leftNodes.Get(i), TcpSocketFactory::GetTypeId());
        tcpSocket->SetAttribute("SndBufSize", UintegerValue(131072));
        tcpSocket->SetAttribute("RcvBufSize", UintegerValue(131072));
        // Congestion window tracing for sender 0 only
        if (i == 0)
        {
            std::string cwndTraceFile = queueDiscType + "-tcp-cwnd.txt";
            tcpSocket->TraceConnect("CongestionWindow", MakeBoundCallback(&CwndTracer, cwndTraceFile));
        }
        OnOffHelper client("ns3::TcpSocketFactory", tcpSinkAddress);
        client.SetAttribute("Remote", AddressValue(tcpSinkAddress));
        client.SetAttribute("PacketSize", UintegerValue(1000));
        client.SetAttribute("DataRate", DataRateValue(DataRate("4Mbps")));
        client.SetAttribute("MaxBytes", UintegerValue(0));
        client.SetAttribute("StartTime", TimeValue(Seconds(0.01 * i + 0.1)));
        client.SetAttribute("StopTime", TimeValue(Seconds(simTime)));
        ApplicationContainer app = client.Install(leftNodes.Get(i));
        tcpApps.Add(app);
    }

    // UDP sender: last left node
    uint32_t udpIndex = nLeft-1;
    Address udpSinkAddress(InetSocketAddress(rightIfs[0].GetAddress(0), udpPort));
    PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory", udpSinkAddress);
    ApplicationContainer udpSinkApp = udpSinkHelper.Install(rightNodes.Get(0));
    udpSinkApp.Start(Seconds(0.0));
    udpSinkApp.Stop(Seconds(simTime + 1));
    OnOffHelper udpClient("ns3::UdpSocketFactory", udpSinkAddress);
    udpClient.SetAttribute("Remote", AddressValue(udpSinkAddress));
    udpClient.SetAttribute("PacketSize", UintegerValue(1000));
    udpClient.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    udpClient.SetAttribute("StartTime", TimeValue(Seconds(0.25)));
    udpClient.SetAttribute("StopTime", TimeValue(Seconds(simTime-1.0)));
    udpApps = udpClient.Install(leftNodes.Get(udpIndex));

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install queue size tracer
    Ptr<QueueDisc> q = qDiscs.Get(0);
    std::string queueTraceFile = queueDiscType + "-queue-size.txt";
    Simulator::Schedule(Seconds(0.0), &QueueDiscTracer, queueTraceFile, q);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    experiment("cobalt");
    experiment("codel");
    return 0;
}