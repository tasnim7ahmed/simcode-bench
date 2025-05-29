#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetAodvRandomWaypointExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create 5 nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Set up Wi-Fi with 802.11b standard
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Set up mobility: RandomWaypointMobilityModel in 100x100 m area
    MobilityHelper mobility;
    mobility.SetMobilityModel(
        "ns3::RandomWaypointMobilityModel",
        "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
        "PositionAllocator", 
            StringValue("ns3::RandomRectanglePositionAllocator["
                        "X=ns3::UniformRandomVariable[Min=0.0|Max=100.0],"
                        "Y=ns3::UniformRandomVariable[Min=0.0|Max=100.0]]")
    );
    mobility.Install(nodes);

    // Install the Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP Server on node 4
    uint16_t udpPort = 4000;
    UdpServerHelper server(udpPort);
    ApplicationContainer serverApps = server.Install(nodes.Get(4));
    serverApps.Start(Seconds(2.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Client on node 0, sending to node 4
    UdpClientHelper client(interfaces.GetAddress(4), udpPort);
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(Seconds(0.025))); // 40 packets/sec
    client.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable routing table logging (optional)
    // Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
    // aodv.PrintRoutingTableAllAt(Seconds(5.0), routingStream);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}