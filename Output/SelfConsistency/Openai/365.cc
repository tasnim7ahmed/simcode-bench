#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for debugging (optional)
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Separate into STA and AP
    NodeContainer staNode = NodeContainer(wifiNodes.Get(0));
    NodeContainer apNode = NodeContainer(wifiNodes.Get(1));

    // Configure Wi-Fi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set up Wi-Fi parameters (use 802.11g, 54Mbps)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;

    // Set the data mode to 54Mbps (OFDM)
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("ErpOfdmRate54Mbps"),
                                "ControlMode", StringValue("ErpOfdmRate24Mbps"));

    // Set SSID
    Ssid ssid = Ssid("ns3-wifi");

    // Configure STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, staNode);

    // Configure AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, apNode);

    // Mobility model
    MobilityHelper mobility;
    // STA position
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // STA
    positionAlloc->Add(Vector(5.0, 0.0, 0.0)); // AP

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer staInterface;
    Ipv4InterfaceContainer apInterface;

    staInterface = address.Assign(staDevice);
    address.NewNetwork();
    apInterface = address.Assign(apDevice);

    // Applications: UDP Server (on AP), Client (on STA)
    uint16_t udpPort = 4000;

    // UDP Server on AP
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApp = udpServer.Install(apNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client on STA: send to AP's IP
    UdpClientHelper udpClient(apInterface.GetAddress(0), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(320)); // e.g., 40 packets per second for 8s.
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.025))); // 40 packets/sec
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(staNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing for inspection (optional)
    // phy.EnablePcap("wifi-simple-sta-ap", apDevice.Get(0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}