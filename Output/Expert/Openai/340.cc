#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    NetDeviceContainer devices;
    // AP: node 0, STA: node 1
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    devices.Add(wifi.Install(phy, mac, wifiNodes.Get(1)));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    devices.Add(wifi.Install(phy, mac, wifiNodes.Get(0)));

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

    InternetStackHelper stack;
    stack.Install(wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    uint32_t maxPackets = 100;
    uint32_t packetSize = 512;
    double interval = 0.1; // seconds

    UdpClientHelper client(interfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(wifiNodes.Get(1));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}