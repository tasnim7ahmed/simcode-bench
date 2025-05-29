#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetOlsrRandomWaypoint");

int main(int argc, char *argv[])
{
    uint32_t numNodes = 6;
    double simTime = 10.0;
    double txPower = 16.0206; // default transmit power in dBm

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Wifi setup
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.Set("TxPowerStart", DoubleValue(txPower));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txPower));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility
    MobilityHelper mobility;
    Ptr<UniformRandomVariable> speed = CreateObject<UniformRandomVariable>();
    speed->SetAttribute("Min", DoubleValue(5.0));
    speed->SetAttribute("Max", DoubleValue(15.0));

    Ptr<UniformRandomVariable> pause = CreateObject<UniformRandomVariable>();
    pause->SetAttribute("Min", DoubleValue(0.0));
    pause->SetAttribute("Max", DoubleValue(0.0));

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", PointerValue(speed),
                             "Pause", PointerValue(pause),
                             "PositionAllocator", 
                                PointerValue(CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"))));
    mobility.Install(nodes);

    // Internet stack + OLSR
    OlsrHelper olsr;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsr);
    internet.Install(nodes);

    // IP addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Logging OLSR routing tables at intervals
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();

        Simulator::Schedule(Seconds(2.0), &Ipv4RoutingProtocol::PrintRoutingTable, 
            ipv4->GetRoutingProtocol(), routingStream);

        Simulator::Schedule(Seconds(5.0), &Ipv4RoutingProtocol::PrintRoutingTable, 
            ipv4->GetRoutingProtocol(), routingStream);

        Simulator::Schedule(Seconds(8.0), &Ipv4RoutingProtocol::PrintRoutingTable, 
            ipv4->GetRoutingProtocol(), routingStream);
    }

    // UDP traffic: node 0 -> node 5
    uint16_t port = 8000;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(5));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(5), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}