#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include <fstream>

using namespace ns3;

static std::ofstream g_tokenTraceFile;

void TokenCountTrace1(uint32_t oldValue, uint32_t newValue)
{
    g_tokenTraceFile << Simulator::Now().GetSeconds() << "\tFirstBucketTokens\t" << newValue << std::endl;
}

void TokenCountTrace2(uint32_t oldValue, uint32_t newValue)
{
    g_tokenTraceFile << Simulator::Now().GetSeconds() << "\tSecondBucketTokens\t" << newValue << std::endl;
}

int main (int argc, char *argv[])
{
    double simTime = 10.0;

    std::string dataRate = "10Mbps";
    std::string delay = "2ms";
    std::string tbfRate = "5Mbps";
    std::string tbfPeakRate = "6Mbps";
    uint32_t tbfBurstSize = 1500*8*10; // 10 packets
    uint32_t tbfPeakBurstSize = 1500*8*3; // 3 packets

    CommandLine cmd;
    cmd.AddValue ("DataRate", "PointToPoint link DataRate", dataRate);
    cmd.AddValue ("Delay", "PointToPoint link Delay", delay);
    cmd.AddValue ("TbfRate", "TBF token arrival rate", tbfRate);
    cmd.AddValue ("TbfPeakRate", "TBF peak rate", tbfPeakRate);
    cmd.AddValue ("TbfBurstSize", "TBF first bucket size (in bits)", tbfBurstSize);
    cmd.AddValue ("TbfPeakBurstSize", "TBF second bucket size (in bits)", tbfPeakBurstSize);
    cmd.AddValue ("SimTime", "Simulation time in seconds", simTime);
    cmd.Parse (argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer devices = p2p.Install(nodes);

    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc ("ns3::TbfQueueDisc",
                                            "Rate", DataRateValue(DataRate(tbfRate)),
                                            "BurstSize", UintegerValue(tbfBurstSize),
                                            "PeakRate", DataRateValue(DataRate(tbfPeakRate)),
                                            "PeakBurstSize", UintegerValue(tbfPeakBurstSize));
    QueueDiscContainer qdiscs = tch.Install(devices.Get(0));
    Ptr<QueueDisc> tbf = qdiscs.Get(0);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 5000;
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onoff.SetAttribute("DataRate", StringValue("8Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1500));

    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simTime));

    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    apps.Add(sinkHelper.Install(nodes.Get(1)));

    g_tokenTraceFile.open("tbf-token-trace.txt");

    Simulator::Schedule(Seconds(0.0), [&]() {
        Ptr<Object> tbfDisc = tbf;
        // To access tokens in first and second buckets, we use Config paths
        // First bucket
        Config::ConnectWithoutContext(
            "/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/RootQueueDisc/TbfQueueDisc/Tokens",
            MakeCallback(&TokenCountTrace1));
        // Second bucket
        Config::ConnectWithoutContext(
            "/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/RootQueueDisc/TbfQueueDisc/PeakTokens",
            MakeCallback(&TokenCountTrace2));
    });

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    std::cout << "TBF statistics at the end of simulation:" << std::endl;
    std::cout << *DynamicCast<TbfQueueDisc>(tbf) << std::endl;

    g_tokenTraceFile.close();
    Simulator::Destroy();
    return 0;
}