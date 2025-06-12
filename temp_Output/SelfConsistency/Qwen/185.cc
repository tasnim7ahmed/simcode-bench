#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiMultiAPSimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable rts cts
    Config::SetDefault("ns3::WifiMac::RtsThreshold", UintegerValue(9000));

    NodeContainer apNodes;
    apNodes.Create(3);

    NodeContainer staNodes;
    staNodes.Create(6);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHz);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    WifiMacHelper mac;
    Ssid ssid;

    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;

    for (uint32_t i = 0; i < apNodes.GetN(); ++i)
    {
        ssid = Ssid("wifi-network-" + std::to_string(i));
        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconInterval", TimeValue(Seconds(2.0)));
        apDevices.Add(wifi.Install(phy, mac, apNodes.Get(i)));
    }

    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        uint32_t apIndex = i % apNodes.GetN(); // Distribute STAs across APs
        ssid = Ssid("wifi-network-" + std::to_string(apIndex));
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid));
        staDevices.Add(wifi.Install(phy, mac, staNodes.Get(i)));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterfaces;
    Ipv4InterfaceContainer staInterfaces;
    Ipv4InterfaceContainer serverInterface;

    apInterfaces = address.Assign(apDevices);
    staInterfaces = address.Assign(staDevices);

    // Create a remote server node
    NodeContainer serverNode;
    serverNode.Create(1);
    stack.Install(serverNode);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer serverDevice;
    serverDevice = p2p.Install(serverNode.Get(0), apNodes.Get(0)); // Connect server to first AP

    Ipv4InterfaceContainer serverP2PInterface = address.Assign(serverDevice);
    serverInterface.Add(serverP2PInterface.Get(0));

    // Set up UDP flows from stations to the server
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(serverInterface.GetAddress(0), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer apps;

    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        apps.Add(onoff.Install(staNodes.Get(i)));
    }

    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Install UDP echo server
    UdpEchoServerHelper echoServer(port);
    apps = echoServer.Install(serverNode.Get(0));
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(10.0));

    // Animation
    AnimationInterface anim("wifi_multi_ap.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Optional: Color nodes
    for (uint32_t i = 0; i < apNodes.GetN(); ++i)
    {
        anim.UpdateNodeDescription(apNodes.Get(i), "AP-" + std::to_string(i));
        anim.UpdateNodeColor(apNodes.Get(i), 255, 0, 0); // Red for APs
    }
    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        anim.UpdateNodeDescription(staNodes.Get(i), "STA-" + std::to_string(i));
        anim.UpdateNodeColor(staNodes.Get(i), 0, 0, 255); // Blue for STAs
    }
    anim.UpdateNodeDescription(serverNode.Get(0), "Server");
    anim.UpdateNodeColor(serverNode.Get(0), 0, 255, 0); // Green for Server

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}