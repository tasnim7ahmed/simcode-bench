#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("OlsrRoutingProtocol", LOG_LEVEL_INFO);

    uint32_t numNodes = 6;
    double simTime = 10.0; // seconds

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Mobility: RandomWaypoint within 100x100m, min speed 5 m/s, max 15 m/s, pause 0s
    MobilityHelper mobility;
    ObjectFactory pos;
    pos.SetTypeId("ns3::RandomRectanglePositionAllocator");
    pos.Set("X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    pos.Set("Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    Ptr<PositionAllocator> positionAlloc = pos.Create()->GetObject<PositionAllocator>();

    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"),
                             "PositionAllocator", PointerValue(positionAlloc));
    mobility.Install(nodes);

    // Wifi: YANS
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Internet stack with OLSR
    InternetStackHelper internet;
    OlsrHelper olsr;
    internet.SetRoutingHelper(olsr);
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP traffic from node 0 to node 5
    uint16_t port = 4000;
    // UDP Server on node 5
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(5));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // UDP Client on node 0
    UdpClientHelper client(interfaces.GetAddress(5), port);
    client.SetAttribute("MaxPackets", UintegerValue(1));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}