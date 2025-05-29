#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FourNodeTopology");

void
RxPacketCounter(Ptr<const Packet> packet, const Address &address)
{
    static uint32_t rxPackets = 0;
    rxPackets++;
    NS_LOG_UNCOND("Received packet #" << rxPackets << " at " << Simulator::Now().GetSeconds() << "s");
}

int
main(int argc, char *argv[])
{
    uint32_t metricN1N3 = 10;
    std::string queueTraceFile = "queue-trace.tr";
    std::string recvTraceFile = "recv-trace.tr";
    double simStopTime = 5.0;

    CommandLine cmd;
    cmd.AddValue("metricN1N3", "Routing metric for the n1-n3 link", metricN1N3);
    cmd.AddValue("queueTraceFile", "Output file for queue tracing", queueTraceFile);
    cmd.AddValue("recvTraceFile", "Output file for packet reception tracing", recvTraceFile);
    cmd.Parse(argc, argv);

    LogComponentEnable("FourNodeTopology", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4); // 0=n0, 1=n1, 2=n2, 3=n3

    // Create node containers for each link
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NodeContainer n1n3 = NodeContainer(nodes.Get(1), nodes.Get(3));
    NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3));

    // Create and configure point-to-point links
    PointToPointHelper p2p_5mb_2ms;
    p2p_5mb_2ms.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_5mb_2ms.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p_1p5mb_100ms;
    p2p_1p5mb_100ms.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_1p5mb_100ms.SetChannelAttribute("Delay", StringValue("100ms"));

    PointToPointHelper p2p_1p5mb_10ms;
    p2p_1p5mb_10ms.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_1p5mb_10ms.SetChannelAttribute("Delay", StringValue("10ms"));

    // Install devices and channels
    NetDeviceContainer dev_n0n2 = p2p_5mb_2ms.Install(n0n2);
    NetDeviceContainer dev_n1n2 = p2p_5mb_2ms.Install(n1n2);
    NetDeviceContainer dev_n1n3 = p2p_1p5mb_100ms.Install(n1n3);
    NetDeviceContainer dev_n2n3 = p2p_1p5mb_10ms.Install(n2n3);

    // Queue Tracing
    AsciiTraceHelper asciiTraceHelper;
    p2p_5mb_2ms.EnableAsciiAll(asciiTraceHelper.CreateFileStream(queueTraceFile));
    p2p_1p5mb_100ms.EnableAsciiAll(asciiTraceHelper.CreateFileStream(queueTraceFile, std::ios::app));
    p2p_1p5mb_10ms.EnableAsciiAll(asciiTraceHelper.CreateFileStream(queueTraceFile, std::ios::app));

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n0n2 = address.Assign(dev_n0n2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n1n2 = address.Assign(dev_n1n2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n1n3 = address.Assign(dev_n1n3);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n2n3 = address.Assign(dev_n2n3);

    // Routing: use GlobalRouting
    Ipv4GlobalRoutingHelper globalRouting;
    Ptr<Ipv4StaticRouting> staticRoutingN1 = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(nodes.Get(1)->GetObject<Ipv4>()->GetRoutingProtocol());
    // Set interface metric for n1 <---> n3 link (at n1)
    Ptr<Ipv4> ipv4n1 = nodes.Get(1)->GetObject<Ipv4>();
    uint32_t ifIndexN1N3 = ipv4n1->GetInterfaceForDevice(dev_n1n3.Get(0));
    staticRoutingN1->SetMetric(ifIndexN1N3, metricN1N3);

    // Recompute global routes after metric change
    Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

    // Install applications
    uint16_t udpPort = 7000;
    // Create packet sink on n1 to receive traffic, bind to n1's address on n1n3 link to ensure accessibility from n3
    Address sinkAddr(InetSocketAddress(if_n1n3.GetAddress(0), udpPort));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simStopTime));

    // Configure UDP traffic from n3 to n1
    uint32_t maxPacketCount = 1000;
    uint32_t packetSize = 1024; // bytes
    double interPacketInterval = 0.01; // seconds (i.e. 100 packets/sec)
    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddr);
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("MaxBytes", UintegerValue(maxPacketCount * packetSize));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer app = onoff.Install(nodes.Get(3));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(simStopTime));

    // Packet reception tracing on n1
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    Ptr<OutputStreamWrapper> recvStream = asciiTraceHelper.CreateFileStream(recvTraceFile);
    sink->TraceConnectWithoutContext("Rx", MakeBoundCallback([recvStream](Ptr<const Packet> packet, const Address &addr){
        *recvStream->GetStream() << Simulator::Now().GetSeconds()
                                << " Received packet of size " << packet->GetSize() << " bytes from " << addr << std::endl;
    }));

    Simulator::Stop(Seconds(simStopTime));
    Simulator::Run();

    // Log simulation results
    uint64_t totalRx = sink->GetTotalRx();
    NS_LOG_UNCOND("Total Bytes Received at n1: " << totalRx);
    NS_LOG_UNCOND("Simulation finished at " << Simulator::Now().GetSeconds() << "s");

    Simulator::Destroy();
    return 0;
}