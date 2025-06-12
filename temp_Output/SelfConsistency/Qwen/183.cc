#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TwoApFourStaUdpNetwork");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable rts/c ts
    Config::SetDefault("ns3::WifiMac::RtsCtsThreshold", UintegerValue(1500));

    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer staNodes;
    staNodes.Create(4);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
    wifi.SetRemoteStationManager("ns3::AarfcdRemoteStationManager");

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("AP1-Network");
    Ssid ssid2 = Ssid("AP2-Network");

    // Setup APs
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

    // Setup STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = wifi.Install(phy, mac, staNodes.Get(0));
    NetDeviceContainer staDevices2 = wifi.Install(phy, mac, staNodes.Get(1));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices3 = wifi.Install(phy, mac, staNodes.Get(2));
    NetDeviceContainer staDevices4 = wifi.Install(phy, mac, staNodes.Get(3));

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;

    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);

    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
    Ipv4InterfaceContainer staInterfaces3 = address.Assign(staDevices3);
    Ipv4InterfaceContainer staInterfaces4 = address.Assign(staDevices4);

    // UDP traffic: setup applications
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(apInterfaces1.GetAddress(0), port));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer appSta1 = onoff.Install(staNodes.Get(0));
    appSta1.Start(Seconds(1.0));
    appSta1.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkAppAp1 = sink.Install(apNodes.Get(0));
    sinkAppAp1.Start(Seconds(0.0));

    onoff.SetAttribute("Remote", InetSocketAddress(apInterfaces2.GetAddress(0), port));
    ApplicationContainer appSta3 = onoff.Install(staNodes.Get(2));
    appSta3.Start(Seconds(1.0));
    appSta3.Stop(Seconds(10.0));

    ApplicationContainer sinkAppAp2 = sink.Install(apNodes.Get(1));
    sinkAppAp2.Start(Seconds(0.0));

    // Animation output
    AnimationInterface anim("two-ap-four-sta-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Add node descriptions for visualization
    for (uint32_t i = 0; i < apNodes.GetN(); ++i) {
        anim.UpdateNodeDescription(apNodes.Get(i), "AP");
        anim.UpdateNodeColor(apNodes.Get(i), 255, 0, 0); // Red
    }
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        anim.UpdateNodeDescription(staNodes.Get(i), "STA");
        anim.UpdateNodeColor(staNodes.Get(i), 0, 0, 255); // Blue
    }

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}