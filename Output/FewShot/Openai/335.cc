#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable TCP echo logging
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create nodes: 1 STA (Client), 1 AP
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure Wi-Fi PHY and MAC
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;

    // STA uses random walk within 100x100m area
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(10.0, 10.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                             "Distance", DoubleValue(10),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Mode", StringValue("Time"),
                             "Time", TimeValue(Seconds(1.0)));
    mobility.Install(wifiStaNode);

    // AP is fixed at (50,50)
    MobilityHelper apMobility;
    Ptr<ListPositionAllocator> apPosAlloc = CreateObject<ListPositionAllocator>();
    apPosAlloc->Add(Vector(50.0, 50.0, 0.0));
    apMobility.SetPositionAllocator(apPosAlloc);
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    apMobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    // IP address assignment
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface, apInterface;
    staInterface = address.Assign(staDevice);
    apInterface = address.Assign(apDevice);

    // TCP Sink (server) on AP, port 9
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(apInterface.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(15.0));

    // TCP source (client) on STA: send 10 packets of 1024 bytes, 1s apart
    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("DataRate", StringValue("8kbps")); // value unimportant for OnTime/OffTime
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.01]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.99]"));
    onoff.SetAttribute("MaxBytes", UintegerValue(10 * 1024));

    ApplicationContainer clientApp = onoff.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(15.0));

    // Enable pcap
    phy.EnablePcapAll("wifi-tcp");

    Simulator::Stop(Seconds(15.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}