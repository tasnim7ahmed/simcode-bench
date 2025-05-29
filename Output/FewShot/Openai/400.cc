#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging (optional)
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create two Wi-Fi nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Setup Wi-Fi channel and physical layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set up Wi-Fi MAC/layer
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-wifi-ssid");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

    // Set up mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(staDevice, apDevice));

    // Install TCP server on node 1 (AP)
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = packetSinkHelper.Install(wifiNodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Install TCP client on node 0 (STA)
    OnOffHelper client("ns3::TcpSocketFactory", sinkAddress);
    client.SetAttribute("DataRate", DataRateValue(DataRate("81920bps"))); // 1024 bytes every 100ms = ~81.92 kbps
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Not used in OnOffHelper for TCP, use DataRate+PacketSize

    ApplicationContainer clientApp = client.Install(wifiNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}