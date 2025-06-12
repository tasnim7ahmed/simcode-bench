#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Nodes: 1 AP + 2 STAs
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Wi-Fi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Wi-Fi configuration (802.11ac)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);
    WifiMacHelper mac;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("VhtMcs8"),
                                "ControlMode", StringValue("VhtMcs8"));

    Ssid ssid = Ssid("ns3-80211ac");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility for STAs: random movement in 50x50 area
    MobilityHelper mobilitySta;
    mobilitySta.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0.0, 50.0, 0.0, 50.0)));
    mobilitySta.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobilitySta.Install(wifiStaNodes);

    // AP is stationary, at center
    MobilityHelper mobilityAp;
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.SetPositionAllocator("ns3::ListPositionAllocator",
                                  "PositionList", VectorValue(Vector(25.0, 25.0, 0.0)));
    mobilityAp.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // IP address assignment
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Application - TCP server (PacketSink) on STA 0
    uint16_t port = 5000;
    Address sinkAddress(InetSocketAddress(staInterfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiStaNodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Application - TCP client (BulkSend) on STA 1 to STA 0
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0));
    bulkSend.SetAttribute("SendSize", UintegerValue(1024));
    ApplicationContainer sourceApps = bulkSend.Install(wifiStaNodes.Get(1));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    // Traffic control: set the data rate to 100Mbps at the client
    Config::Set("/NodeList/1/ApplicationList/0/$ns3::BulkSendApplication/MaxBytes", UintegerValue(0));
    Config::Set("/NodeList/1/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TcpSocketFactory::GetTypeId()));
    // We control the offered load using a traffic control module here or by shaping at BulkSend

    // Enable PCAP tracing
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("sta0", staDevices.Get(0));
    phy.EnablePcap("sta1", staDevices.Get(1));
    phy.EnablePcap("ap", apDevice.Get(0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}