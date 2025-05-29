#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(3);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, nodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, nodes.Get(1));

    NetDeviceContainer adhocDevices = wifi.Install(phy, mac, nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(staDevices);
    interfaces = address.Assign(apDevices);
    interfaces = address.Assign(adhocDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    UdpServerHelper server(port);
    server.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    ApplicationContainer apps = server.Install(nodes.Get(2));

    UdpClientHelper client(interfaces.GetAddress(2), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("StartTime", TimeValue(Seconds(2.0)));

    apps = client.Install(nodes.Get(0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}