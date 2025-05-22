#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiNetworkTest");

void TestWifiStaNodesCreation()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // Two stations
    NS_ASSERT_MSG(wifiStaNodes.GetN() == 2, "Expected 2 Wifi STA nodes.");
}

void TestWifiApNodeCreation()
{
    NodeContainer wifiApNode;
    wifiApNode.Create(1); // One access point
    NS_ASSERT_MSG(wifiApNode.GetN() == 1, "Expected 1 Wifi AP node.");
}

void TestWifiInstallation()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // Two stations

    NodeContainer wifiApNode;
    wifiApNode.Create(1); // One access point

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    NS_ASSERT_MSG(staDevices.GetN() == 2, "Expected 2 WiFi STA devices.");
    NS_ASSERT_MSG(apDevice.GetN() == 1, "Expected 1 WiFi AP device.");
}

void TestMobilityInstallation()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // Two stations

    NodeContainer wifiApNode;
    wifiApNode.Create(1); // One access point

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    Ptr<MobilityModel> apMobility = wifiApNode.Get(0)->GetObject<MobilityModel>();
    NS_ASSERT_MSG(apMobility != nullptr, "AP mobility model is not set.");

    Ptr<MobilityModel> staMobility = wifiStaNodes.Get(0)->GetObject<MobilityModel>();
    NS_ASSERT_MSG(staMobility != nullptr, "STA mobility model is not set.");
}

void TestUdpServerClientApplication()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // Two stations

    NodeContainer wifiApNode;
    wifiApNode.Create(1); // One access point

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Install UDP application
    uint16_t port = 9; // UDP port

    // Create UDP server on one of the stations
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiStaNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Create UDP client on the other station
    UdpClientHelper udpClient(staInterfaces.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(320));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05))); // 20 packets per second
    udpClient.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes per packet

    ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    NS_ASSERT_MSG(udpServer.GetReceived() > 0, "Expected UDP server to receive packets.");
}

void TestNetAnim()
{
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // Two stations

    NodeContainer wifiApNode;
    wifiApNode.Create(1); // One access point

    AnimationInterface anim("wifi_network.xml");
    anim.SetConstantPosition(wifiApNode.Get(0), 50.0, 50.0); // AP position
    anim.SetConstantPosition(wifiStaNodes.Get(0), 30.0, 30.0); // STA 1 position
    anim.SetConstantPosition(wifiStaNodes.Get(1), 70.0, 30.0); // STA 2 position

    Simulator::Run();
    Simulator::Destroy();

    // Check if animation file has been created (this is a basic test, but more checks can be added)
    std::ifstream f("wifi_network.xml");
    NS_ASSERT_MSG(f.good(), "NetAnim XML file was not generated.");
}

int main(int argc, char *argv[])
{
    TestWifiStaNodesCreation();
    TestWifiApNodeCreation();
    TestWifiInstallation();
    TestMobilityInstallation();
    TestUdpServerClientApplication();
    TestNetAnim();

    return 0;
}
