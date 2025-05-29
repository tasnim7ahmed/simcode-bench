#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Set up Wi-Fi phy and MAC
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility: Random position and movement
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", TimeValue(Seconds(1.0)),
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=2.0]"),
                             "Bounds", RectangleValue(Rectangle(0,50,0,50)));
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP Server on node 4
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(4));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client on node 0
    UdpClientHelper client(interfaces.GetAddress(4), port);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}