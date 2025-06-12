#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetAodvExample");

int main(int argc, char *argv[])
{
    uint32_t numNodes = 5;
    double duration = 10.0;
    double startApplication = 2.0;
    double areaSize = 100.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("duration", "Simulation duration in seconds", duration);
    cmd.AddValue("areaSize", "Side length of square area in meters", areaSize);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaSize) + "]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaSize) + "]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.2]"));
    mobility.Install(nodes);

    // Configure Wi-Fi
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    wifiPhy.SetStandard(WIFI_STANDARD_80211b);

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifiPhy.Install(wifiMac, nodes);

    // Install internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP echo server on node 4
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(duration));

    // Setup UDP client on node 0 sending to node 4
    UdpEchoClientHelper echoClient(interfaces.GetAddress(4), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(startApplication));
    clientApps.Stop(Seconds(duration));

    // Enable PCAP tracing
    wifiPhy.EnablePcapAll("manet-aodv");

    // Set simulation stop time
    Simulator::Stop(Seconds(duration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}