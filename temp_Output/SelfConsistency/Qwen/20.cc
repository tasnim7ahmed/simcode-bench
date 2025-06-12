#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BlockAckSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: AP + 2 STAs
    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    // Setup YANS channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Configure MAC for 802.11n and Block Ack
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("blockack-network");

    // Configure block ack parameters
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue("HtMcs7"),
                                  "ControlMode", StringValue("HtMcs0"));

    // Install devices
    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(MicroSeconds(102400)),
                "EnableBlockAck", BooleanValue(true),
                "BlockAckThreshold", UintegerValue(2),
                "BlockAckInactivityTimeout", TimeValue(Seconds(10)));
    apDevice = wifi.Install(phy, mac, wifiApNode);

    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false),
                "EnableBlockAck", BooleanValue(true),
                "BlockAckThreshold", UintegerValue(2),
                "BlockAckInactivityTimeout", TimeValue(Seconds(10)));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);
    address.Assign(staDevices);

    // Point-to-point link to capture ICMP replies
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NodeContainer p2pNodes;
    p2pNodes.Create(2);
    NetDeviceContainer p2pDevices;
    p2pDevices = p2p.Install(p2pNodes);

    // Assign IP addresses to P2P nodes
    address.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign(p2pDevices);

    // Connect AP to P2P network via bridge
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4> apIpv4 = wifiApNode.Get(0)->GetObject<Ipv4>();
    uint32_t apIfIndex = apIpv4->AddInterface(p2pNodes.Get(0));
    Ipv4InterfaceAddress apP2pAddr = Ipv4InterfaceAddress("10.1.1.1", "255.255.255.252");
    apIpv4->AddAddress(apIfIndex, apP2pAddr);
    apIpv4->SetMetric(apIfIndex, 1);
    apIpv4->SetUp(apIfIndex);

    // Global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Applications: UDP sender from STA1 to AP
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(apInterface.GetAddress(0), port)));
    onoff.SetConstantRate(DataRate("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer appSta1 = onoff.Install(wifiStaNodes.Get(0));
    appSta1.Start(Seconds(1.0));
    appSta1.Stop(Seconds(10.0));

    // Packet sink on AP
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(11.0));

    // Enable PCAP at AP
    phy.EnablePcap("ap-wifi-device", apDevice.Get(0));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}