#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsr-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANET_UDP_DSR_SUMO");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(100));

    NodeContainer nodes;
    nodes.Create(10);

    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"), "ControlMode", StringValue("OfdmRate6Mbps"));

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    InternetStackHelper stack;
    DsrMainHelper dsrMain;
    dsrMain.Install(stack, nodes);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(nodes);

    V4PingHelper ping(interfaces.GetAddress(1));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(512));
    ApplicationContainer apps = ping.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(80.0));

    AsciiTraceHelper ascii;
    wifiPhy.EnableAsciiAll(ascii.CreateFileStream("vanet_udp_dsr_sumo.tr"));
    wifiMac.EnableAsciiAll(ascii.CreateFileStream("vanet_udp_dsr_sumo.tr"));
    AnimationInterface anim("vanet_udp_dsr_sumo.xml");

    Simulator::Stop(Seconds(80.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}