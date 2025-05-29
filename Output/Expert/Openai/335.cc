#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set default simulation parameters
    uint32_t packetSize = 1024;
    uint32_t numPackets = 10;
    double interval = 1.0; // seconds
    uint16_t port = 9;

    Time::SetResolution(Time::NS);

    // Create nodes: 0 = STA (Client), 1 = AP
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Configure Wi-Fi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");

    // Configure STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

    // Configure AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

    // Set mobility
    MobilityHelper mobility;

    // STA - RandomWalk2dMobilityModel in 100x100
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)));
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(10.0),
                                  "MinY", DoubleValue(10.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(wifiNodes.Get(0));

    // AP - ConstantPositionMobilityModel
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes.Get(1));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staIp = address.Assign(staDevice);
    Ipv4InterfaceContainer apIp = address.Assign(apDevice);

    // Install TCP server on AP (port 9)
    Address apLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", apLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiNodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    // Install TCP client on STA
    OnOffHelper clientHelper("ns3::TcpSocketFactory",
                             InetSocketAddress(apIp.GetAddress(0), port));
    clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("10Mbps")));

    // Each packet at a 1s interval (OnTime/OffTime settings)
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.01]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.99]"));

    ApplicationContainer clientApp = clientHelper.Install(wifiNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(20.0));

    // Simulation Run
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}