#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging (optional, uncomment for debugging)
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    // LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create nodes: 1 AP, 2 STA
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Set up Wi-Fi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure 802.11ac/Wi-Fi 5
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211ac");

    // Set up STA devices
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    WifiPhyHelper phySta = phy;
    phySta.Set("ChannelNumber", UintegerValue(36)); // 5GHz

    NetDeviceContainer staDevices = wifi.Install(phySta, mac, wifiStaNodes);

    // Set up AP device
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    WifiPhyHelper phyAp = phy;
    phyAp.Set("ChannelNumber", UintegerValue(36));

    NetDeviceContainer apDevice = wifi.Install(phyAp, mac, wifiApNode);

    // Mobility configuration
    MobilityHelper mobility;

    // STA nodes: RandomWalk2dMobility within 50x50
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(wifiStaNodes);

    // AP: stationary at center
    MobilityHelper mobilityAp;
    Ptr<ListPositionAllocator> apPos = CreateObject<ListPositionAllocator>();
    apPos->Add(Vector(25.0, 25.0, 0.0));
    mobilityAp.SetPositionAllocator(apPos);
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(wifiApNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // TCP server app on STA 0 (port 5000)
    uint16_t port = 5000;
    Address sinkAddress(InetSocketAddress(staInterfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = packetSinkHelper.Install(wifiStaNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // TCP client app on STA 1
    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer clientApp = onoff.Install(wifiStaNodes.Get(1));

    // Enable pcap tracing
    phySta.EnablePcap("wifi_sta", staDevices, true);
    phyAp.EnablePcap("wifi_ap", apDevice, true);

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}