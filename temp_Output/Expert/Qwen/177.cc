#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpCongestionControlDumbbell");

int main(int argc, char *argv[]) {
    std::string transportProt = "TcpReno";
    std::string ecnMode = "NoEcn";
    uint32_t numFlows = 2;
    uint32_t maxBytes = 0;
    uint32_t payloadSize = 1448;
    Time interPacketInterval = MilliSeconds(0.5);
    Time simulationDuration = Seconds(10.0);

    CommandLine cmd(__FILE__);
    cmd.AddValue("transportProt", "Transport protocol to use: TcpReno, TcpCubic", transportProt);
    cmd.AddValue("ecnMode", "ECN mode: NoEcn, EcnCapable, EcnEnabled", ecnMode);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::" + transportProt)));
    Config::SetDefault("ns3::TcpSocketBase::EcnMode", StringValue(ecnMode));

    NodeContainer leftNodes;
    leftNodes.Create(numFlows);

    NodeContainer rightNode;
    rightNode.Create(1);

    NodeContainer bottleneckNodes;
    bottleneckNodes.Add(leftNodes.Get(0));
    bottleneckNodes.Add(rightNode.Get(0));

    for (uint32_t i = 1; i < numFlows; ++i) {
        bottleneckNodes.Add(leftNodes.Get(i));
        bottleneckNodes.Add(rightNode.Get(0));
    }

    PointToPointHelper p2pLeft;
    p2pLeft.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pLeft.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pBottleneck.SetChannelAttribute("Delay", StringValue("10ms"));
    p2pBottleneck.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("50p"));

    NetDeviceContainer leftDevices[numFlows];
    NetDeviceContainer bottleneckDevices[2 * numFlows];

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    for (uint32_t i = 0; i < numFlows; ++i) {
        leftDevices[i] = p2pLeft.Install(leftNodes.Get(i), bottleneckNodes.Get(2 * i));
        address.Assign(leftDevices[i]);
        address.NewNetwork();
    }

    for (uint32_t i = 0; i < numFlows; ++i) {
        bottleneckDevices[i] = p2pBottleneck.Install(bottleneckNodes.Get(2 * i), bottleneckNodes.Get(2 * i + 1));
        address.Assign(bottleneckDevices[i]);
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);

    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < numFlows; ++i) {
        sinkApps.Add(sinkHelper.Install(rightNode.Get(0)));
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(simulationDuration);

    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numFlows; ++i) {
        AddressValue remoteAddress(InetSocketAddress(rightNode.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), port));
        clientHelper.SetAttribute("Remote", remoteAddress);
        clientApps.Add(clientHelper.Install(leftNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(simulationDuration - Seconds(1.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("tcp-congestion-control.tr");
    p2pBottleneck.EnableAsciiAll(stream);

    AnimationInterface anim("tcp-congestion-animation.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("route-trace.xml", Seconds(0), simulationDuration, Seconds(0.25));

    Simulator::Stop(simulationDuration);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}