#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging for UdpClient and UdpServer if needed
    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    Time::SetResolution(Time::NS);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi-ssid");

    // Set up STA devices
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Set up AP device
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility for AP: stationary
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    Ptr<MobilityModel> apMobility = wifiApNode.Get(0)->GetObject<MobilityModel>();
    apMobility->SetPosition(Vector(0.0, 0.0, 0.0));

    // Mobility for STAs: random walk 2D
    MobilityHelper staMobility;
    staMobility.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    staMobility.Install(wifiStaNodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // UDP server on the AP node
    uint16_t udpPort = 4000;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // UDP client on STA 0
    uint32_t maxPacketCount = 100;
    Time interPacketInterval = Seconds(0.1); // 10 packets/sec, over 10s = 100 pks
    UdpClientHelper udpClient(apInterface.GetAddress(0), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing (optional)
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifi-ap", apDevice.Get(0));
    phy.EnablePcap("wifi-sta0", staDevices.Get(0));
    phy.EnablePcap("wifi-sta1", staDevices.Get(1));
    phy.EnablePcap("wifi-sta2", staDevices.Get(2));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}