#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for UDP echo applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Setup WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility setup - arrange nodes in a line for clarity
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 50.0, 0.0));    // Node 0
    positionAlloc->Add(Vector(50.0, 50.0, 0.0));   // Node 1 (relay)
    positionAlloc->Add(Vector(100.0, 50.0, 0.0));  // Node 2
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack with AODV routing
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP server on node 2
    uint16_t port = 9009;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // UDP client on node 0 sending to node 2, must be routed through node 1
    UdpEchoClientHelper client(interfaces.GetAddress(2), port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    // NetAnim setup
    AnimationInterface anim("aodv-adhoc-netanim.xml");
    anim.SetConstantPosition(nodes.Get(0), 0.0, 50.0);
    anim.SetConstantPosition(nodes.Get(1), 50.0, 50.0);
    anim.SetConstantPosition(nodes.Get(2), 100.0, 50.0);

    // Show which node is which in the animation
    anim.UpdateNodeDescription(nodes.Get(0), "Node 0 (Client)");
    anim.UpdateNodeDescription(nodes.Get(1), "Node 1 (Relay)");
    anim.UpdateNodeDescription(nodes.Get(2), "Node 2 (Server)");
    anim.UpdateNodeColor(nodes.Get(0), 0, 255, 0);
    anim.UpdateNodeColor(nodes.Get(1), 0, 0, 255);
    anim.UpdateNodeColor(nodes.Get(2), 255, 0, 0);

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}