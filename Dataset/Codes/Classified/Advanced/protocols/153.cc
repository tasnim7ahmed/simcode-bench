#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvRoutingExample");

int main(int argc, char *argv[])
{
    LogComponentEnable("AodvRoutingExample", LOG_LEVEL_INFO);

    // Create 4 nodes: node 0, node 1, node 2, and node 3
    NodeContainer nodes;
    nodes.Create(4);

    // Configure WiFi channel and physical layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set up the WiFi MAC and install on nodes
    WifiHelper wifi;
    WifiMacHelper mac;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    mac.SetType("ns3::AdhocWifiMac");  // Ad-hoc network

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Set up mobility model to arrange nodes in a random 100x100 area
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install the AODV routing protocol using the InternetStackHelper
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);  // Set AODV as the routing protocol
    internet.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up a UDP Echo server on node 3
    UdpEchoServerHelper echoServer(9);  // Port 9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up a UDP Echo client on node 0 to send traffic to node 3
    UdpEchoClientHelper echoClient(interfaces.GetAddress(3), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing to capture the traffic
    phy.EnablePcapAll("aodv-routing-4-nodes");

    // Run the simulation for 10 seconds
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // End the simulation
    Simulator::Destroy();
    return 0;
}

