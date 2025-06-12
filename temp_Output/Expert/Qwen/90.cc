#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/error-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimulationScript");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);
    NodeContainer n0n2(nodes.Get(0), nodes.Get(2));
    NodeContainer n1n2(nodes.Get(1), nodes.Get(2));
    NodeContainer n3n2(nodes.Get(3), nodes.Get(2));

    PointToPointHelper p2p_n0n2;
    p2p_n0n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_n0n2.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p_n1n2;
    p2p_n1n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_n1n2.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p_n3n2;
    p2p_n3n2.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_n3n2.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer d_n0n2 = p2p_n0n2.Install(n0n2);
    NetDeviceContainer d_n1n2 = p2p_n1n2.Install(n1n2);
    NetDeviceContainer d_n3n2 = p2p_n3n2.Install(n3n2);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n0n2 = address.Assign(d_n0n2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n1n2 = address.Assign(d_n1n2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n3n2 = address.Assign(d_n3n2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    // CBR over UDP from n0 to n3
    UdpClientHelper udpClient_n0_n3(i_n3n2.GetAddress(0), port);
    udpClient_n0_n3.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    udpClient_n0_n3.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
    udpClient_n0_n3.SetAttribute("PacketSize", UintegerValue(210));

    ApplicationContainer app_udp_n0_n3 = udpClient_n0_n3.Install(nodes.Get(0));
    app_udp_n0_n3.Start(Seconds(0.0));
    app_udp_n0_n3.Stop(Seconds(2.0));

    UdpServerHelper udpServer_n3(port);
    ApplicationContainer app_udpserver_n3 = udpServer_n3.Install(nodes.Get(3));
    app_udpserver_n3.Start(Seconds(0.0));
    app_udpserver_n3.Stop(Seconds(2.0));

    // CBR over UDP from n3 to n1
    UdpClientHelper udpClient_n3_n1(i_n1n2.GetAddress(1), port);
    udpClient_n3_n1.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    udpClient_n3_n1.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
    udpClient_n3_n1.SetAttribute("PacketSize", UintegerValue(210));

    ApplicationContainer app_udp_n3_n1 = udpClient_n3_n1.Install(nodes.Get(3));
    app_udp_n3_n1.Start(Seconds(0.0));
    app_udp_n3_n1.Stop(Seconds(2.0));

    UdpServerHelper udpServer_n1(port);
    ApplicationContainer app_udpserver_n1 = udpServer_n1.Install(nodes.Get(1));
    app_udpserver_n1.Start(Seconds(0.0));
    app_udpserver_n1.Stop(Seconds(2.0));

    // FTP over TCP from n0 to n3 (short burst)
    BulkSendHelper ftpApp("ns3::TcpSocketFactory", InetSocketAddress(i_n3n2.GetAddress(0), port));
    ftpApp.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer app_ftp_n0_n3 = ftpApp.Install(nodes.Get(0));
    app_ftp_n0_n3.Start(Seconds(1.2));
    app_ftp_n0_n3.Stop(Seconds(1.35));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer app_sink_n3 = sink.Install(nodes.Get(3));
    app_sink_n3.Start(Seconds(0.0));
    app_sink_n3.Stop(Seconds(2.0));

    // Error models setup
    Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
    rem->SetAttribute("ErrorRate", DoubleValue(0.001));
    rem->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

    Ptr<BurstErrorModel> bem = CreateObject<BurstErrorModel>();
    bem->SetAttribute("ErrorRate", DoubleValue(0.01));
    bem->SetAttribute("BurstSize", ConstantVariableWithStreamValue(2));

    Ptr<ListErrorModel> lem = CreateObject<ListErrorModel>();
    std::list<uint64_t> dropList = {11, 17};
    for (auto uid : dropList) {
        lem->Add(uid);
    }

    // Install error models on appropriate devices
    d_n0n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(rem));
    d_n3n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(bem));
    d_n1n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(lem));

    AsciiTraceHelper ascii;
    p2p_n0n2.EnableAsciiAll(ascii.CreateFileStream("simple-error-model.tr"));
    p2p_n1n2.EnableAsciiAll(ascii.CreateFileStream("simple-error-model.tr"));
    p2p_n3n2.EnableAsciiAll(ascii.CreateFileStream("simple-error-model.tr"));

    Simulator::Stop(Seconds(2.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}