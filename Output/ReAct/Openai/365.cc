#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Create nodes: one AP, one STA
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Set up physical layer and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure the Wi-Fi helper
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate54Mbps"),
                                 "ControlMode", StringValue("OfdmRate24Mbps"));

    // Set up SSID
    Ssid ssid = Ssid("ns3-wifi");

    // Device and MAC helper for STA
    WifiMacHelper macSta;
    macSta.SetType("ns3::StaWifiMac",
                   "Ssid", SsidValue(ssid),
                   "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, macSta, wifiStaNode);

    // Device and MAC helper for AP
    WifiMacHelper macAp;
    macAp.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, macAp, wifiApNode);

    // Set mobility: Place STA and AP statically
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (5.0),
                                   "DeltaY", DoubleValue (0.0),
                                   "GridWidth", UintegerValue (2),
                                   "LayoutType", StringValue ("RowFirst"));

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNode);
    mobility.Install (wifiApNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface;
    staInterface = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // UDP application: PacketSink on AP (port 9)
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(apInterface.GetAddress(0), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // UDP client on STA
    UdpClientHelper client(sinkAddress);
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing (optional, can be commented out)
    // phy.EnablePcap("wifi-udp", apDevice.Get(0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}