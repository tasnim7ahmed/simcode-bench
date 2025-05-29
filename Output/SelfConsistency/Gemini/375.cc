#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiApStaRandomWalk");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
    LogComponent::SetLogLevel(
        "WifiApStaRandomWalk", LOG_LEVEL_ALL); // Enable logging for this component

    NodeContainer nodes;
    nodes.Create(2);

    // Configure AP (Node 1) mobility: Stationary
    MobilityHelper mobilityAp;
    mobilityAp.SetPositionAllocator("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(nodes.Get(0));

    // Configure STA (Node 2) mobility: Random Walk 2D
    MobilityHelper mobilitySta;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(1.0, 1.0, 0.0));
    mobilitySta.SetPositionAllocator(positionAlloc);
    mobilitySta.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
        "Mode", EnumValue(RandomWalk2dMobilityModel::MODE_TIME),
        "Time", StringValue("2s"),
        "Speed", StringValue("1m/s"),
        "Bounds", RectangleValue(Rectangle(-10, 10, -10, 10)));
    mobilitySta.Install(nodes.Get(1));

    // Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // Configure AP
    mac.SetType("ns3::ApWifiMac",
        "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, nodes.Get(0));

    // Configure STA
    mac.SetType("ns3::StaWifiMac",
        "Ssid", SsidValue(ssid),
        "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, nodes.Get(1));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(NetDeviceContainer(apDevice, staDevice));

    // Configure UDP client-server application
    UdpClientServerHelper echoClientServer(9);
    echoClientServer.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClientServer.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClientServer.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClientServer.Install(nodes.Get(1), nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}