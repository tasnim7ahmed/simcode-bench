#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

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

    PointToPointHelper p2p_n1n2;
    p2p_n1n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_n1n2.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p_n2n3;
    p2p_n2n3.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_n2n3.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer d_n0n2 = p2p_n0n2.Install(n0, n2);
    NetDeviceContainer d_n1n2 = p2p_n1n2.Install(n1, n2);
    NetDeviceContainer d_n2n3 = p2p_n2n3.Install(n2, n3);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n0n2 = address.Assign(d_n0n2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n1n2 = address.Assign(d_n1n2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n2n3 = address.Assign(d_n2n3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP/CBR flow from n0 to n3
    uint16_t cbrPort = 9;
    OnOffHelper onoff_udp("ns3::UdpSocketFactory", Address(InetSocketAddress(i_n2n3.GetAddress(1), cbrPort)));
    onoff_udp.SetConstantRate(DataRate("448Kbps"), 210);
    onoff_udp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff_udp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer app_udp_n0 = onoff_udp.Install(n0);
    app_udp_n0.Start(Seconds(0.0));
    app_udp_n0.Stop(Seconds(2.0));

    PacketSinkHelper sink_udp("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), cbrPort)));
    ApplicationContainer app_sink_udp = sink_udp.Install(n3);
    app_sink_udp.Start(Seconds(0.0));
    app_sink_udp.Stop(Seconds(2.0));

    // UDP/CBR flow from n3 to n1
    OnOffHelper onoff_udp_rev("ns3::UdpSocketFactory", Address(InetSocketAddress(i_n1n2.GetAddress(1), cbrPort)));
    onoff_udp_rev.SetConstantRate(DataRate("448Kbps"), 210);
    onoff_udp_rev.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff_udp_rev.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer app_udp_n3 = onoff_udp_rev.Install(n3);
    app_udp_n3.Start(Seconds(0.0));
    app_udp_n3.Stop(Seconds(2.0));

    PacketSinkHelper sink_udp_rev("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), cbrPort)));
    ApplicationContainer app_sink_udp_rev = sink_udp_rev.Install(n1);
    app_sink_udp_rev.Start(Seconds(0.0));
    app_sink_udp_rev.Stop(Seconds(2.0));

    // TCP/FTP flow from n0 to n3
    uint16_t ftpPort = 21;
    InetSocketAddress sinkAddr_tcp(i_n2n3.GetAddress(1), ftpPort);
    AddressValue remoteAddress(sinkAddr_tcp);

    BulkSendHelper ftp("ns3::TcpSocketFactory", remoteAddress);
    ftp.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer app_tcp = ftp.Install(n0);
    app_tcp.Start(Seconds(1.2));
    app_tcp.Stop(Seconds(1.35));

    PacketSinkHelper sink_tcp("ns3::TcpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), ftpPort)));
    ApplicationContainer app_sink_tcp = sink_tcp.Install(n3);
    app_sink_tcp.Start(Seconds(0.0));
    app_sink_tcp.Stop(Seconds(2.0));

    // Error models
    RateErrorModel rateEm;
    rateEm.SetAttribute("ErrorRate", DoubleValue(0.001));
    BurstErrorModel burstEm;
    burstEm.SetAttribute("ErrorRate", DoubleValue(0.01));

    ListErrorModel listEm;
    std::vector<uint64_t> packetsToDrop = {11, 17};
    for (auto uid : packetsToDrop) {
        listEm.Add(uid);
    }

    // Combine error models
    ComposedErrorModel em;
    em.Add(&rateEm);
    em.Add(&burstEm);
    em.Add(&listEm);

    // Apply error model to the outgoing device of n2 towards n3
    d_n2n3.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(&em));

    AsciiTraceHelper asciiTraceHelper;
    p2p_n0n2.EnableAsciiAll(asciiTraceHelper.CreateFileStream("simple-error-model.tr"));
    p2p_n1n2.EnableAsciiAll(asciiTraceHelper.CreateFileStream("simple-error-model.tr"));
    p2p_n2n3.EnableAsciiAll(asciiTraceHelper.CreateFileStream("simple-error-model.tr"));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}