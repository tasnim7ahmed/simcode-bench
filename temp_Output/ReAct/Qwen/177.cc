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

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    std::string transportProt = "TcpReno";
    cmd.AddValue("transport_prot", "Transport protocol to use: TcpReno, TcpCubic", transportProt);
    cmd.Parse(argc, argv);

    if (transportProt.compare("TcpReno") != 0 && transportProt.compare("TcpCubic") != 0)
    {
        std::cout << "Invalid transport protocol specified. Use TcpReno or TcpCubic." << std::endl;
        return 1;
    }

    // Enable required TCP variants
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                       TypeIdValue(TypeId::LookupByName("ns3::" + transportProt)));

    NodeContainer nodesLeft;
    nodesLeft.Create(2);

    NodeContainer router;
    router.Create(2);

    NodeContainer nodesRight;
    nodesRight.Create(2);

    NodeContainer leftRouter(nodesLeft.Get(0), router.Get(0));
    NodeContainer rightRouter(nodesRight.Get(0), router.Get(1));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devLeftRouter = p2p.Install(leftRouter);
    NetDeviceContainer devRightRouter = p2p.Install(rightRouter);

    NodeContainer bottleneck(router.Get(0), router.Get(1));
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1.5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    NetDeviceContainer devBottleneck = p2p.Install(bottleneck);

    PointToPointHelper p2pOut;
    p2pOut.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2pOut.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devLeftClient = p2pOut.Install(nodesLeft.Get(0), nodesLeft.Get(1));
    NetDeviceContainer devRightClient = p2pOut.Install(nodesRight.Get(0), nodesRight.Get(1));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifLeftRouter = address.Assign(devLeftRouter);

    address.NewNetwork();
    Ipv4InterfaceContainer ifRightRouter = address.Assign(devRightRouter);

    address.NewNetwork();
    Ipv4InterfaceContainer ifBottleneck = address.Assign(devBottleneck);

    address.NewNetwork();
    Ipv4InterfaceContainer ifLeftClient = address.Assign(devLeftClient);

    address.NewNetwork();
    Ipv4InterfaceContainer ifRightClient = address.Assign(devRightClient);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;

    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodesRight.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    Address sinkLocalAddress2(InetSocketAddress(Ipv4Address::GetAny(), port + 1));
    PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", sinkLocalAddress2);
    ApplicationContainer sinkApp2 = sinkHelper2.Install(nodesRight.Get(1));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(10.0));

    InetSocketAddress rmtAddr(ifRightClient.GetAddress(1), port);
    rmtAddr.SetTos(0xb8); // IPTOS_LOWDELAY | IPTOS_THROUGHPUT

    BulkSendHelper source("ns3::TcpSocketFactory", rmtAddr);
    source.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApp = source.Install(nodesLeft.Get(1));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(9.0));

    InetSocketAddress rmtAddr2(ifRightClient.GetAddress(1), port + 1);
    rmtAddr2.SetTos(0x0);

    BulkSendHelper source2("ns3::TcpSocketFactory", rmtAddr2);
    source2.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApp2 = source2.Install(nodesLeft.Get(1));
    sourceApp2.Start(Seconds(2.0));
    sourceApp2.Stop(Seconds(8.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("tcp-congestion.tr");
    stack.EnablePcapIpv4(stream, ifBottleneck.GetAddress(0));

    AnimationInterface anim("tcp-congestion.xml");
    anim.SetConstantPosition(nodesLeft.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(router.Get(0), 50.0, 0.0);
    anim.SetConstantPosition(router.Get(1), 100.0, 0.0);
    anim.SetConstantPosition(nodesRight.Get(0), 150.0, 0.0);
    anim.SetConstantPosition(nodesLeft.Get(1), 0.0, 20.0);
    anim.SetConstantPosition(nodesRight.Get(1), 150.0, 20.0);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}