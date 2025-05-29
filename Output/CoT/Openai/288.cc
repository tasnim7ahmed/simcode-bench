#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes: 1 AP, 2 STAs
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure WiFi 802.11ac (VhtWifi)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211ac");

    // STA configuration
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP configuration
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiApNode);

    // Mobility: AP stationary at center, STAs random in 50x50
    MobilityHelper mobility;

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(25.0, 25.0, 0.0)); // AP in center
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(wifiStaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterfaces;
    apInterfaces = address.Assign(apDevices);

    // TCP server (PacketSink) on STA 0, port 5000
    uint16_t port = 5000;
    Address sinkAddress(InetSocketAddress(staInterfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = packetSinkHelper.Install(wifiStaNodes.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // TCP client on STA 1 -> sends to STA 0
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", sinkAddress);
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    onOffHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onOffHelper.SetAttribute("StopTime", TimeValue(Seconds(10.0)));

    ApplicationContainer clientApps = onOffHelper.Install(wifiStaNodes.Get(1));

    // Enable PCAP tracing
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifi_ap", apDevices);
    phy.EnablePcap("wifi_sta0", staDevices.Get(0));
    phy.EnablePcap("wifi_sta1", staDevices.Get(1));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}