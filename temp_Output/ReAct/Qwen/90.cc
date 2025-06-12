#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimulationScript");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p1;
    p2p1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p1.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p2;
    p2p2.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p2.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices0_2 = p2p1.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devices1_2 = p2p1.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices3_2 = p2p2.Install(nodes.Get(3), nodes.Get(2));

    Ptr<RateErrorModel> rateEm = CreateObject<RateErrorModel>();
    rateEm->SetAttribute("ErrorRate", DoubleValue(0.001));
    rateEm->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

    Ptr<BurstErrorModel> burstEm = CreateObject<BurstErrorModel>();
    burstEm->SetAttribute("ErrorRate", DoubleValue(0.01));
    burstEm->SetAttribute("BurstSize", ConstantVariableWithStreamValue(3));

    Ptr<ListErrorModel> listEm = CreateObject<ListErrorModel>();
    std::list<uint64_t> pktuids;
    pktuids.push_back(11);
    pktuids.push_back(17);
    listEm->SetList(pktuids);

    devices3_2.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(listEm));
    devices3_2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(rateEm));
    devices0_2.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(burstEm));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces0_2 = address.Assign(devices0_2);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces1_2 = address.Assign(devices1_2);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces3_2 = address.Assign(devices3_2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    OnOffHelper onoff1("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces3_2.GetAddress(1), port)));
    onoff1.SetConstantRate(DataRate("448Kbps"), 210);
    onoff1.SetAttribute("StartTime", TimeValue(Seconds(0.0)));
    onoff1.SetAttribute("StopTime", TimeValue(Seconds(2.0)));

    ApplicationContainer appOnOff1 = onoff1.Install(nodes.Get(0));
    appOnOff1.Start(Seconds(0.0));
    appOnOff1.Stop(Seconds(2.0));

    OnOffHelper onoff2("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces1_2.GetAddress(1), port)));
    onoff2.SetConstantRate(DataRate("448Kbps"), 210);
    onoff2.SetAttribute("StartTime", TimeValue(Seconds(0.0)));
    onoff2.SetAttribute("StopTime", TimeValue(Seconds(2.0)));

    ApplicationContainer appOnOff2 = onoff2.Install(nodes.Get(3));
    appOnOff2.Start(Seconds(0.0));
    appOnOff2.Stop(Seconds(2.0));

    BulkSendHelper ftp("ns3::TcpSocketFactory", InetSocketAddress(interfaces3_2.GetAddress(1), port + 1));
    ftp.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer appFtp = ftp.Install(nodes.Get(0));
    appFtp.Start(Seconds(1.2));
    appFtp.Stop(Seconds(1.35));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 1));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(3));
    sinkApp.Start(Seconds(1.2));
    sinkApp.Stop(Seconds(1.35));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("simple-error-model.tr");
    p2p1.EnableAsciiAll(stream);
    p2p2.EnableAsciiAll(stream);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}