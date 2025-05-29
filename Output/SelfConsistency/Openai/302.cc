#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging (optional)
    // LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi for 802.11g
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("ErpOfdmRate54Mbps"), "ControlMode", StringValue("ErpOfdmRate24Mbps"));

    // MAC helpers
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // STA MACs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP MAC
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Internet stack and IP addresses
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // UDP Server on STA 0 (Node 0)
    uint16_t serverPort = 8080;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApp = udpServer.Install(wifiStaNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client on STA 1 (Node 1) sending to STA 0
    UdpClientHelper udpClient(staInterfaces.GetAddress(0), serverPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing (optional)
    // phy.EnablePcap("wifi-80211g-ap", apDevice.Get(0));
    // phy.EnablePcap("wifi-80211g-sta0", staDevices.Get(0));
    // phy.EnablePcap("wifi-80211g-sta1", staDevices.Get(1));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}