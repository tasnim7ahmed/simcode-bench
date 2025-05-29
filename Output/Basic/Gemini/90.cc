#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/error-model.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint02;
    pointToPoint02.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint02.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint02.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

    PointToPointHelper pointToPoint32;
    pointToPoint32.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    pointToPoint32.SetChannelAttribute("Delay", StringValue("10ms"));
    pointToPoint32.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

    NetDeviceContainer devices02 = pointToPoint02.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devices12 = pointToPoint02.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices32 = pointToPoint32.Install(nodes.Get(3), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces32 = address.Assign(devices32);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP flow n0 -> n3
    uint16_t port03 = 9;
    UdpClientHelper client03(interfaces32.GetAddress(0), port03);
    client03.SetAttribute("MaxPackets", UintegerValue(1000000));
    client03.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
    client03.SetAttribute("PacketSize", UintegerValue(210));
    ApplicationContainer clientApps03 = client03.Install(nodes.Get(0));
    clientApps03.Start(Seconds(1.0));
    clientApps03.Stop(Seconds(1.5));

    UdpServerHelper server03(port03);
    ApplicationContainer serverApps03 = server03.Install(nodes.Get(3));
    serverApps03.Start(Seconds(0.9));
    serverApps03.Stop(Seconds(1.6));

    // UDP flow n3 -> n1
    uint16_t port31 = 9;
    UdpClientHelper client31(interfaces12.GetAddress(0), port31);
    client31.SetAttribute("MaxPackets", UintegerValue(1000000));
    client31.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
    client31.SetAttribute("PacketSize", UintegerValue(210));
    ApplicationContainer clientApps31 = client31.Install(nodes.Get(3));
    clientApps31.Start(Seconds(1.0));
    clientApps31.Stop(Seconds(1.5));

    UdpServerHelper server31(port31);
    ApplicationContainer serverApps31 = server31.Install(nodes.Get(1));
    serverApps31.Start(Seconds(0.9));
    serverApps31.Stop(Seconds(1.6));

    // TCP flow n0 -> n3
    uint16_t port03_tcp = 50000;
    BulkSendHelper source03("ns3::TcpSocketFactory", InetSocketAddress(interfaces32.GetAddress(0), port03_tcp));
    source03.SetAttribute("SendSize", UintegerValue(1400));
    ApplicationContainer sourceApps03 = source03.Install(nodes.Get(0));
    sourceApps03.Start(Seconds(1.2));
    sourceApps03.Stop(Seconds(1.35));

    PacketSinkHelper sink03("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port03_tcp));
    ApplicationContainer sinkApps03 = sink03.Install(nodes.Get(3));
    sinkApps03.Start(Seconds(1.1));
    sinkApps03.Stop(Seconds(1.4));

    // Error Models
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(0.001));
    em->SetAttribute("RandomVariable", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"));
    devices02.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    Ptr<BurstErrorModel> bem = CreateObject<BurstErrorModel>();
    bem->SetAttribute("ErrorRate", DoubleValue(0.01));
    bem->SetAttribute("BurstSize", StringValue("1"));
    devices12.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(bem));

    Ptr<ListErrorModel> lem = CreateObject<ListErrorModel>();
    lem->SetAttribute("ErrorUids", StringValue("11,17"));
    devices32.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(lem));

    // Tracing
    AsciiTraceHelper ascii;
    pointToPoint02.EnableAsciiAll(ascii.CreateFileStream("simple-error-model.tr"));

    // Queue Tracing
    Simulator::Schedule(Seconds(0.00001), &TrafficControlHelper::EnableQueueDiscLoggingAll, "simple-error-model-queue");

    // PCAP Tracing
    pointToPoint02.EnablePcapAll("simple-error-model");
    pointToPoint32.EnablePcapAll("simple-error-model");

    Simulator::Stop(Seconds(2.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}