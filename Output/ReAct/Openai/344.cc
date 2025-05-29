#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Log settings (optional, for debugging)
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    // LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create 3 nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(3);

    // Set up grid mobility: 3 nodes in a 3x3 layout (unused positions will not have nodes)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Configure WiFi: 802.11b
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    // Set up 802.11b PHY and channel
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // MAC: all are station devices, no AP
    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-simple");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

    // Install devices
    NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // TCP Server (PacketSink) on node 2 (listening on port 9)
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(2), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer serverApp = packetSinkHelper.Install(wifiNodes.Get(2));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(20.0));

    // TCP clients: node 0 and node 1, sending to node 2
    for (uint32_t i = 0; i < 2; ++i)
    {
        OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
        clientHelper.SetAttribute("PacketSize", UintegerValue(512));
        clientHelper.SetAttribute("MaxBytes", UintegerValue(5120)); // 10 packets * 512 bytes
        clientHelper.SetAttribute("DataRate", StringValue("100Mb/s"));
        clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Not used by OnOff, so manage rate via MaxBytes

        ApplicationContainer clientApp = clientHelper.Install(wifiNodes.Get(i));
        clientApp.Start(Seconds(1.0 + i * 0.1)); // Stagger starts slightly
        clientApp.Stop(Seconds(20.0));
    }

    // Enable pcap tracing
    phy.EnablePcapAll("wifi-tcp-grid");

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}