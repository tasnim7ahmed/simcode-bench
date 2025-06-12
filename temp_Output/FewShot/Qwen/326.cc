#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set default AODV routing protocol
    Config::SetDefault("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue(true));

    // Create 5 nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Setup mobility model for all nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0]"),
                              "PositionAllocator", PointerValue(CreateObject<RandomRectanglePositionAllocator>()));
    RandomRectanglePositionAllocator *positionAlloc = new RandomRectanglePositionAllocator();
    positionAlloc->SetX(ObjectFactory("ns3::UniformRandomVariable", "Min", DoubleValue(0.0), "Max", DoubleValue(100.0)));
    positionAlloc->SetY(ObjectFactory("ns3::UniformRandomVariable", "Min", DoubleValue(0.0), "Max", DoubleValue(100.0)));
    mobility.Install(nodes);

    // Install Wi-Fi with 802.11b standard
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    wifiPhy.SetStandard(WIFI_STANDARD_80211b);
    wifiPhy.SetChannelAttribute("Frequency", UintegerValue(2412));
    wifiPhy.SetChannelAttribute("ChannelWidth", UintegerValue(5));

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifiPhy.Install(wifiMac, nodes);

    // Install AODV routing protocol
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipAssigner;
    ipAssigner.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipAssigner.Assign(devices);

    // Setup UDP echo server on node 4
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(4));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Setup UDP client on node 0 sending to node 4
    UdpEchoClientHelper client(interfaces.GetAddress(4), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}