#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessTcpAodvSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices;
    devices = wifi.Install(wifiPhy, mac, nodes);

    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    InetSocketAddress inetSinkAddress = InetSocketAddress(interfaces.GetAddress(1), sinkPort);
    OnOffHelper clientApp("ns3::TcpSocketFactory", inetSinkAddress);
    clientApp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientApp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientApp.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientApp.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = clientApp.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("tcp-wireless-aodv.tr");
    wifiPhy.EnableAsciiAll(stream);

    AnimationInterface anim("wireless-tcp-aodv.xml");
    anim.Set MobilityPollInterval(Seconds(1));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(0));
    std::cout << "Total Bytes Received: " << sink->GetTotalRx() << " bytes" << std::endl;
    double throughput = static_cast<double>(sink->GetTotalRx()) * 8 / (10.0 - 1.0) / 1000; // kbps
    std::cout << "Throughput: " << throughput << " kbps" << std::endl;

    Simulator::Destroy();
    return 0;
}