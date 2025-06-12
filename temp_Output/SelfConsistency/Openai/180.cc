#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvAdhocNetAnimExample");

int main(int argc, char *argv[])
{
    // Enable logging for troubleshooting
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Configure Wi-Fi devices/ad-hoc MAC
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Set Mobility Model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(50.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up UDP server (sink) on node 2
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(15.0));

    // UDP client on node 0, destination is node 2 via node 1
    UdpClientHelper client(interfaces.GetAddress(2), port);
    client.SetAttribute("MaxPackets", UintegerValue(20));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(14.0));

    // NetAnim setup
    AnimationInterface anim("aodv-adhoc-netanim.xml");

    // Set node descriptions and colors for clarity
    anim.SetConstantPosition(nodes.Get(0), 0, 50);
    anim.SetConstantPosition(nodes.Get(1), 50, 50);
    anim.SetConstantPosition(nodes.Get(2), 100, 50);

    anim.UpdateNodeDescription(0, "Source");
    anim.UpdateNodeDescription(1, "Relay");
    anim.UpdateNodeDescription(2, "Destination");
    anim.UpdateNodeColor(0, 255, 0, 0);   // Red for source
    anim.UpdateNodeColor(1, 0, 255, 255); // Cyan for relay
    anim.UpdateNodeColor(2, 0, 0, 255);   // Blue for sink

    // Enable packet metadata for NetAnim
    wifi.EnablePcapAll("aodv-adhoc");

    Simulator::Stop(Seconds(16.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}