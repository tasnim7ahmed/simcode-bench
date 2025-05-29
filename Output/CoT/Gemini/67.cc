#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

static void
CwndTracer (std::string filename)
{
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream (filename);
  Config::ConnectWithoutContext ("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow",
                                MakeBoundCallback (&CwndChange, stream));
}

static void
QueueDiscTracer (std::string filename, std::string queueDiscPath)
{
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream (filename);
  Config::ConnectWithoutContext (queueDiscPath + "/Queue/Size",
                                MakeBoundCallback (&QueueSizeChange, stream));
}

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newCwnd << std::endl;
}

static void
QueueSizeChange (Ptr<OutputStreamWrapper> stream, uint32_t oldSize, uint32_t newSize)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newSize << std::endl;
}


void experiment(std::string queueDiscType) {
    uint32_t nSenders = 7;
    double simulationTime = 10.0;
    std::string bottleneckDataRate = "10Mbps";
    std::string bottleneckDelay = "2ms";

    NodeContainer leftNodes, rightNodes, bottleneckLeft, bottleneckRight;
    leftNodes.Create(nSenders);
    rightNodes.Create(1);
    bottleneckLeft.Create(1);
    bottleneckRight.Create(1);

    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(bottleneckLeft);
    stack.Install(bottleneckRight);

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckDataRate));
    bottleneckLink.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    NetDeviceContainer leftBottleneckDevices = bottleneckLink.Install(bottleneckLeft, bottleneckRight);

    PointToPointHelper leftLink;
    leftLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    leftLink.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer leftDevices[nSenders];
    for (uint32_t i = 0; i < nSenders; ++i) {
        NetDeviceContainer temp = leftLink.Install(leftNodes.Get(i), bottleneckLeft);
        leftDevices[i] = temp;
    }

    PointToPointHelper rightLink;
    rightLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    rightLink.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer rightDevices = rightLink.Install(bottleneckRight, rightNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer leftInterfaces[nSenders];
    for (uint32_t i = 0; i < nSenders; ++i) {
        Ipv4InterfaceContainer temp = address.Assign(leftDevices[i]);
        leftInterfaces[i] = temp;
    }
    address.Assign(leftBottleneckDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer rightInterface = address.Assign(rightDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up Queue Disc
    TrafficControlHelper tch;
    tch.SetRootQueueDisc(queueDiscType);

    QueueDiscContainer queueDiscs;
    queueDiscs = tch.Install(leftBottleneckDevices.Get(1));

    // UDP Application
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(rightNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpEchoClientHelper echoClient(rightInterface.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps[nSenders];
    for (uint32_t i = 0; i < nSenders; ++i) {
      clientApps[i] = echoClient.Install(leftNodes.Get(i));
      clientApps[i].Start(Seconds(2.0));
      clientApps[i].Stop(Seconds(simulationTime));
    }

    // TCP Application
    uint16_t sinkPort = 8080;
    Address sinkLocalAddress(InetSocketAddress(Ipv4InterfaceContainer(rightInterface).GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(rightNodes.Get(0));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(simulationTime));

    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(rightInterface.GetAddress(1), sinkPort));
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer sourceApps[nSenders];
    for (uint32_t i = 0; i < nSenders; ++i) {
      sourceApps[i] = bulkSendHelper.Install(leftNodes.Get(i));
      sourceApps[i].Start(Seconds(2.0));
      sourceApps[i].Stop(Seconds(simulationTime));
    }

    std::string queueDiscPath = "/NodeList/" + std::to_string(bottleneckRight.Get(0)->GetId()) + "/TrafficControlLayer/RootQueueDiscList/CoDel[0]";
    if (queueDiscType == "ns3::CobaltQueueDisc") {
        queueDiscPath = "/NodeList/" + std::to_string(bottleneckRight.Get(0)->GetId()) + "/TrafficControlLayer/RootQueueDiscList/Cobalt[0]";
    }
    std::string queueSizeFilename = "queue-size-" + queueDiscType + ".txt";
    QueueDiscTracer(queueSizeFilename, queueDiscPath);

    std::string cwndFilename = "cwnd-" + queueDiscType + ".txt";
    CwndTracer(cwndFilename);


    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
}

int main(int argc, char *argv[]) {
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    experiment("ns3::CobaltQueueDisc");
    experiment("ns3::CoDelQueueDisc");
    return 0;
}