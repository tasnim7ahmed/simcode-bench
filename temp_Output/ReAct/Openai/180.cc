#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution (Time::NS);

    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Install Wifi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));    // Node 0
    positionAlloc->Add(Vector(50.0, 0.0, 0.0));   // Node 1
    positionAlloc->Add(Vector(100.0, 0.0, 0.0));  // Node 2
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP Server on node 2
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client on node 0, destination node 2
    UdpClientHelper client(interfaces.GetAddress(2), port);
    client.SetAttribute("MaxPackets", UintegerValue(50));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(200)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable packet reception/tx tracing for NetAnim
    AnimationInterface anim("aodv-adhoc.xml");
    anim.UpdateNodeColor(0, 0, 255, 0); // Node 0: Blue
    anim.UpdateNodeColor(1, 255, 140, 0); // Node 1: Orange
    anim.UpdateNodeColor(2, 255, 0, 0); // Node 2: Red

    // Animation interface packet metadata enables per-packet visualization
    AnimationInterface::SetConstantPosition(nodes.Get(0), 0.0, 0.0);
    AnimationInterface::SetConstantPosition(nodes.Get(1), 50.0, 0.0);
    AnimationInterface::SetConstantPosition(nodes.Get(2), 100.0, 0.0);

    // Enable tx/rx trace sources (for packet anim in NetAnim)
    phy.EnablePcapAll("aodv-adhoc");

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}