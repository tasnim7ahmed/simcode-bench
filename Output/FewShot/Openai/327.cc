#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable OLSR logging and routing logs
    LogComponentEnable("OlsrRoutingProtocol", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    uint32_t numNodes = 6;
    double simTime = 10.0;

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Configure WiFi ad-hoc
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility: RandomWaypointMobilityModel
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"),
                             "PositionAllocator", StringValue("ns3::RandomRectanglePositionAllocator[MinX=0.0|MinY=0.0|MaxX=100.0|MaxY=100.0]"));
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "MaxX", DoubleValue(100.0),
                                 "MaxY", DoubleValue(100.0));
    mobility.Install(nodes);

    // Install Internet stack and OLSR
    OlsrHelper olsr;
    InternetStackHelper stack;
    stack.SetRoutingHelper(olsr);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo server on node 5
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(5));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // UDP Echo client on node 0 -> node 5
    UdpEchoClientHelper echoClient(interfaces.GetAddress(5), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Enable routing table printing
    Ipv4StaticRoutingHelper staticRouting;
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);

    Simulator::Schedule(Seconds(1.0), [&](){
        std::cout << "\nRouting tables at t=1s\n";
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            olsr.PrintRoutingTable(ipv4, routingStream);
        }
    });

    Simulator::Schedule(Seconds(5.0), [&](){
        std::cout << "\nRouting tables at t=5s\n";
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            olsr.PrintRoutingTable(ipv4, routingStream);
        }
    });

    Simulator::Schedule(Seconds(9.0), [&](){
        std::cout << "\nRouting tables at t=9s\n";
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            olsr.PrintRoutingTable(ipv4, routingStream);
        }
    });

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}