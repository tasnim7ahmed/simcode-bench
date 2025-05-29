#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create 2 nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    // Configure WiFi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // WiFi 802.11b with Aarf Rate Manager
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // Both nodes act as STA (no AP separating them)
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer devices = wifi.Install(phy, mac, wifiStaNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    Ptr<MobilityModel> mob0 = wifiStaNodes.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> mob1 = wifiStaNodes.Get(1)->GetObject<MobilityModel>();
    mob0->SetPosition(Vector(0.0, 0.0, 0.0));
    mob1->SetPosition(Vector(5.0, 10.0, 0.0));

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Server: TCP sink port 9 on Node 1
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = packetSinkHelper.Install(wifiStaNodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(20.0));

    // Client: OnOffApplication on Node 0
    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("PacketSize", UintegerValue(512));
    clientHelper.SetAttribute("DataRate", StringValue("40960bps")); // 512 bytes every 0.1s = 40960bps
    clientHelper.SetAttribute("MaxBytes", UintegerValue(100 * 512)); // 100 packets * 512 bytes

    ApplicationContainer clientApps = clientHelper.Install(wifiStaNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(20.0));

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}