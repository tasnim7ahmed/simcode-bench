#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTwoApFourStaSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable packet tracing in NetAnim
    bool enablePcap = false;
    cmd.AddValue("pcap", "Enable pcap capture", enablePcap);
    cmd.Parse(argc, argv);

    // Create 2 APs and 4 STAs
    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer staNodes;
    staNodes.Create(4);

    // Create wifi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Setup MAC and PHY for APs and STAs
    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHz);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    // Set up SSID
    Ssid ssid1 = Ssid("network-AP1");
    Ssid ssid2 = Ssid("network-AP2");

    // Install APs
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1),
                "BeaconInterval", TimeValue(Seconds(2.0)));
    NetDeviceContainer apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2),
                "BeaconInterval", TimeValue(Seconds(2.0)));
    NetDeviceContainer apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

    // Install STAs - they will associate with nearest AP automatically
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = mac.Install(phy, staNodes.Get(0));
    NetDeviceContainer staDevices2 = mac.Install(phy, staNodes.Get(1));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices3 = mac.Install(phy, staNodes.Get(2));
    NetDeviceContainer staDevices4 = mac.Install(phy, staNodes.Get(3));

    NetDeviceContainer staDevices;
    staDevices.Add(staDevices1);
    staDevices.Add(staDevices2);
    staDevices.Add(staDevices3);
    staDevices.Add(staDevices4);

    // Mobility models
    MobilityHelper mobility;

    // APs at fixed positions
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(100.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);

    // STAs randomly moving within a rectangular area
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(staNodes);

    // Assign IP addresses
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterfaces;
    apInterfaces.Add(address.Assign(apDevices1));
    apInterfaces.Add(address.Assign(apDevices2));

    address.NewNetwork();
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Setup UDP traffic: All stations send to their respective APs
    uint16_t port = 9;

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(apInterfaces.GetAddress(0), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer apps;

    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        Address apAddr;
        if (i < 2) {
            apAddr = InetSocketAddress(apInterfaces.GetAddress(0), port); // AP1
        } else {
            apAddr = InetSocketAddress(apInterfaces.GetAddress(1), port); // AP2
        }
        onoff.SetAttribute("Remote", AddressValue(apAddr));
        apps.Add(onoff.Install(staNodes.Get(i)));
    }

    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Install packet sink applications on APs to receive traffic
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sink.Install(apNodes);
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(10.0));

    // Enable Pcap if requested
    if (enablePcap) {
        phy.EnablePcapAll("wifi_two_ap_four_sta", false);
    }

    // NetAnim visualization
    AnimationInterface anim("wifi-two-ap-four-sta.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Color nodes
    for (uint32_t i = 0; i < apNodes.GetN(); ++i) {
        anim.UpdateNodeDescription(apNodes.Get(i)->GetId(), "AP");
        anim.UpdateNodeColor(apNodes.Get(i)->GetId(), 255, 0, 0); // Red
    }

    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        anim.UpdateNodeDescription(staNodes.Get(i)->GetId(), "STA");
        anim.UpdateNodeColor(staNodes.Get(i)->GetId(), 0, 0, 255); // Blue
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}