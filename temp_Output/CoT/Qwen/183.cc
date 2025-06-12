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

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: 2 APs and 4 STAs
    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer staNodes;
    staNodes.Create(4);

    // Create Wi-Fi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Set up Wi-Fi MAC and disable rate control (fixed rate)
    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"), "ControlMode", StringValue("OfdmRate6Mbps"));

    // Install APs
    mac.SetType("ns3::ApWifiMac");
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, apNodes);

    // Install STAs
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, staNodes);

    // Create a bridged network between the two APs via point-to-point link
    NodeContainer p2pNodes;
    p2pNodes.Add(apNodes.Get(0));
    p2pNodes.Add(apNodes.Get(1));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices;
    p2pDevices = p2p.Install(p2pNodes);

    // Assign IP addresses
    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;

    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    address.NewNetwork();
    Ipv4InterfaceContainer p2pInterfaces = address.Assign(p2pDevices);

    // Setup UDP Echo server on first STA
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(staNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP Echo client on second STA
    UdpEchoClientHelper echoClient(staInterfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(staNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Mobility model for positioning
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.InstallAll();

    // Enable NetAnim tracing
    AnimationInterface anim("wifi-two-aps-four-stas.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));
    anim.EnablePacketMetadata(true);

    // Start simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}