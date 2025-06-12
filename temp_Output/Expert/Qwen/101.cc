#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/global-route-manager.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleGlobalRouting");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3));

    PointToPointHelper p2p5mb2ms;
    p2p5mb2ms.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p5mb2ms.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    p2p5mb2ms.SetQueue("ns3::DropTailQueue<Packet>");

    PointToPointHelper p2p1_5mb10ms;
    p2p1_5mb10ms.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1.5Mbps")));
    p2p1_5mb10ms.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    p2p1_5mb10ms.SetQueue("ns3::DropTailQueue<Packet>");

    NetDeviceContainer d0d2 = p2p5mb2ms.Install(n0n2);
    NetDeviceContainer d1d2 = p2p5mb2ms.Install(n1n2);
    NetDeviceContainer d2d3 = p2p1_5mb10ms.Install(n2n3);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);
    address.NewNetwork();
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);
    address.NewNetwork();
    Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t cbrPort = 9;
    UdpEchoServerHelper echoServer(cbrPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(2.0));

    ApplicationContainer clientApps;
    UdpEchoClientHelper echoClient(i2i3.GetAddress(1), cbrPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    echoClient.SetAttribute("PacketSize", UintegerValue(210));
    clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(0.0));
    clientApps.Stop(Seconds(2.0));

    echoClient = UdpEchoClientHelper(i1i2.GetAddress(0), cbrPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    echoClient.SetAttribute("PacketSize", UintegerValue(210));
    clientApps = echoClient.Install(nodes.Get(3));
    clientApps.Start(Seconds(0.0));
    clientApps.Stop(Seconds(2.0));

    uint16_t ftpPort = 21;
    Address sinkAddress(InetSocketAddress(i0i2.GetAddress(1), ftpPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), ftpPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(3));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(2.0));

    OnOffHelper onOffHelper("ns3::TcpSocketFactory", sinkAddress);
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("448kbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(210));
    ApplicationContainer sourceApps = onOffHelper.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.2));
    sourceApps.Stop(Seconds(1.35));

    AsciiTraceHelper ascii;
    p2p5mb2ms.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));
    p2p1_5mb10ms.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));

    Simulator::Stop(Seconds(2.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}