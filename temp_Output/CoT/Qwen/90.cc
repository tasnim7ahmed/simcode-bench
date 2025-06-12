#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/error-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);
    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Node> n1 = nodes.Get(1);
    Ptr<Node> n2 = nodes.Get(2);
    Ptr<Node> n3 = nodes.Get(3);

    PointToPointHelper p2p_n0n2;
    p2p_n0n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_n0n2.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p_n0n2.SetQueue("ns3::DropTailQueue<Packet>");

    PointToPointHelper p2p_n1n2;
    p2p_n1n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_n1n2.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p_n1n2.SetQueue("ns3::DropTailQueue<Packet>");

    PointToPointHelper p2p_n3n2;
    p2p_n3n2.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_n3n2.SetChannelAttribute("Delay", StringValue("10ms"));
    p2p_n3n2.SetQueue("ns3::DropTailQueue<Packet>");

    NetDeviceContainer d0d2 = p2p_n0n2.Install(n0, n2);
    NetDeviceContainer d1d2 = p2p_n1n2.Install(n1, n2);
    NetDeviceContainer d3d2 = p2p_n3n2.Install(n3, n2);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);
    address.NewNetwork();
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);
    address.NewNetwork();
    Ipv4InterfaceContainer i3i2 = address.Assign(d3d2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint32_t packetSize = 210;
    double interval = 0.00375;
    double dataRate = (packetSize * 8) / interval / 1000; // in Kbps

    UdpClientHelper udpClient1(i3i2.GetAddress(0), 9);
    udpClient1.SetAttribute("PacketSize", UintegerValue(packetSize));
    udpClient1.SetAttribute("Interval", TimeValue(Seconds(interval)));
    udpClient1.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));

    ApplicationContainer appUdp1 = udpClient1.Install(n0);
    appUdp1.Start(Seconds(0.0));
    appUdp1.Stop(Seconds(2.0));

    PacketSinkHelper sinkUdp1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer sinkAppUdp1 = sinkUdp1.Install(n3);
    sinkAppUdp1.Start(Seconds(0.0));
    sinkAppUdp1.Stop(Seconds(2.0));

    UdpClientHelper udpClient2(i1i2.GetAddress(0), 9);
    udpClient2.SetAttribute("PacketSize", UintegerValue(packetSize));
    udpClient2.SetAttribute("Interval", TimeValue(Seconds(interval)));
    udpClient2.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));

    ApplicationContainer appUdp2 = udpClient2.Install(n3);
    appUdp2.Start(Seconds(0.0));
    appUdp2.Stop(Seconds(2.0));

    PacketSinkHelper sinkUdp2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer sinkAppUdp2 = sinkUdp2.Install(n1);
    sinkAppUdp2.Start(Seconds(0.0));
    sinkAppUdp2.Stop(Seconds(2.0));

    uint16_t port = 21;
    BulkSendHelper ftp("ns3::TcpSocketFactory", InetSocketAddress(i3i2.GetAddress(0), port));
    ftp.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer appFtp = ftp.Install(n0);
    appFtp.Start(Seconds(1.2));
    appFtp.Stop(Seconds(1.35));

    PacketSinkHelper sinkFtp("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkAppFtp = sinkFtp.Install(n3);
    sinkAppFtp.Start(Seconds(1.2));
    sinkAppFtp.Stop(Seconds(1.35));

    RateErrorModel rateEm;
    rateEm.SetRandomVariable(Ptr<UniformRandomVariable>(new UniformRandomVariable()));
    rateEm.SetRate(0.001);

    BurstErrorModel burstEm;
    burstEm.SetRandomVariable(Ptr<UniformRandomVariable>(new UniformRandomVariable()));
    burstEm.SetBurstRate(0.01);

    ListErrorModel listEm;
    listEm.Add(11);
    listEm.Add(17);

    for (uint32_t i = 0; i < d0d2.GetN(); ++i) {
        d0d2.Get(i)->SetAttribute("ReceiveErrorModel", PointerValue(&rateEm));
        d0d2.Get(i)->SetAttribute("TransmitErrorModel", PointerValue(&burstEm));
    }

    for (uint32_t i = 0; i < d3d2.GetN(); ++i) {
        d3d2.Get(i)->SetAttribute("ReceiveErrorModel", PointerValue(&listEm));
        d3d2.Get(i)->SetAttribute("TransmitErrorModel", PointerValue(&rateEm));
    }

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("simple-error-model.tr");

    p2p_n0n2.EnableAsciiAll(stream);
    p2p_n1n2.EnableAsciiAll(stream);
    p2p_n3n2.EnableAsciiAll(stream);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}