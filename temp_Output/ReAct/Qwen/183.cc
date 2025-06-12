#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiTwoAPsFourSTAs");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable rts cts (if needed)
    Config::SetDefault("ns3::WifiMac::RtsThreshold", UintegerValue(999999));

    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer staNodes;
    staNodes.Create(4);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("network-AP1");
    Ssid ssid2 = Ssid("network-AP2");

    // AP MAC settings
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1),
                "BeaconInterval", TimeValue(Seconds(2.0)));
    NetDeviceContainer apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2),
                "BeaconInterval", TimeValue(Seconds(2.0)));
    NetDeviceContainer apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

    // STA MAC settings
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = mac.Install(phy, staNodes.Get(0));
    staDevices1.Add(mac.Install(phy, staNodes.Get(1)));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices2 = mac.Install(phy, staNodes.Get(2));
    staDevices2.Add(mac.Install(phy, staNodes.Get(3)));

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;

    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);

    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);

    // UDP Echo Server and Client setup
    uint16_t port = 9;
    UdpEchoServerHelper server1(port);
    ApplicationContainer apps_server1 = server1.Install(staNodes.Get(0));
    apps_server1.Start(Seconds(1.0));
    apps_server1.Stop(Seconds(10.0));

    UdpEchoClientHelper client1(staInterfaces1.GetAddress(0), port);
    client1.SetAttribute("MaxPackets", UintegerValue(5));
    client1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer apps_client1 = client1.Install(staNodes.Get(2));
    apps_client1.Start(Seconds(2.0));
    apps_client1.Stop(Seconds(10.0));

    UdpEchoServerHelper server2(port);
    ApplicationContainer apps_server2 = server2.Install(staNodes.Get(2));
    apps_server2.Start(Seconds(1.0));
    apps_server2.Stop(Seconds(10.0));

    UdpEchoClientHelper client2(staInterfaces2.GetAddress(0), port);
    client2.SetAttribute("MaxPackets", UintegerValue(5));
    client2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer apps_client2 = client2.Install(staNodes.Get(0));
    apps_client2.Start(Seconds(2.0));
    apps_client2.Stop(Seconds(10.0));

    // Animation
    AnimationInterface anim("wifi-two-aps-four-stas.xml");
    anim.Set MobilityPollInterval(Seconds(1));

    // Add node descriptions for visualization
    for (uint32_t i = 0; i < apNodes.GetN(); ++i) {
        anim.UpdateNodeDescription(apNodes.Get(i), "AP");
    }
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        anim.UpdateNodeDescription(staNodes.Get(i), "STA");
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}