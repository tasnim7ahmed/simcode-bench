#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ssid.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for UdpClient and UdpServer if needed
    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Configure Wi-Fi PHY and channel with fixed delay
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(-30)); // Usually -30 dBm for close proximity

    Ptr<YansWifiChannel> wifiChannel = channel.Create();

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(wifiChannel);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    NetDeviceContainer devices;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    devices.Add(wifi.Install(phy, mac, wifiNodes.Get(0)));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    devices.Add(wifi.Install(phy, mac, wifiNodes.Get(1)));

    // Mobility: grid placement, constant position
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Server on node 1
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(11.0));

    // UDP Client on node 0
    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(320)); // e.g., enough for time period
    client.SetAttribute("Interval", TimeValue(Seconds(0.0256))); // ~10Mbps for 1024B packets
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(wifiNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Configure DataRate and Delay for the PHY, channel (the rate is managed by MAC)
    // The "DataRate" attribute controls the transmission speed at the Wi-Fi MAC/PHY

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("ErpOfdmRate54Mbps"),
                                "ControlMode", StringValue("ErpOfdmRate24Mbps"));

    // Simulate a constant 2ms delay (see propagation delay above)
    wifiChannel->SetPropagationDelayModel(CreateObjectWithAttributes<ConstantSpeedPropagationDelayModel>(
        "Speed", DoubleValue(300000000.0))); // effectively no impact for short distance, but keep as per request

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}