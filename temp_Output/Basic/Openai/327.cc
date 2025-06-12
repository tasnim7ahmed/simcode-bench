#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-table-entry.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetOlsrExample");

void PrintRoutingTableAllNodes(NodeContainer nodes, Time printTime)
{
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        Ipv4RoutingTableEntry route;
        Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol();
        Ptr<olsr::RoutingProtocol> olsr = DynamicCast<olsr::RoutingProtocol>(proto);
        std::cout << "Time " << Simulator::Now().GetSeconds()
                  << "s, Node[" << i << "] Routing Table:\n";
        olsr->PrintRoutingTable(std::cout);
    }
    Simulator::Schedule(printTime, &PrintRoutingTableAllNodes, nodes, printTime);
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    uint32_t numNodes = 6;
    double simTime = 10.0;

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Wifi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility
    MobilityHelper mobility;
    ObjectFactory pos;
    pos.SetTypeId("ns3::UniformRandomVariable");
    pos.Set("Min", DoubleValue(0.0));
    pos.Set("Max", DoubleValue(100.0));

    Ptr<RandomRectanglePositionAllocator> positionAlloc = CreateObject<RandomRectanglePositionAllocator>();
    positionAlloc->SetAttribute("X", PointerValue(pos.Create()->GetObject<RandomVariableStream>()));
    positionAlloc->SetAttribute("Y", PointerValue(pos.Create()->GetObject<RandomVariableStream>()));

    mobility.SetMobilityModel(
        "ns3::RandomWaypointMobilityModel",
        "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"),
        "PositionAllocator", PointerValue(positionAlloc));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(nodes);

    // Internet stack with OLSR
    OlsrHelper olsr;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsr);
    internet.Install(nodes);

    // IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP server on node 5
    uint16_t port = 5000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(nodes.Get(5));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // UDP client on node 0
    UdpClientHelper udpClient(interfaces.GetAddress(5), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = udpClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Print routing table periodically
    Simulator::Schedule(Seconds(0.5), &PrintRoutingTableAllNodes, nodes, Seconds(2.0));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}