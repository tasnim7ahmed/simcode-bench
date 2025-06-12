#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes for mesh topology
    NodeContainer nodes;
    nodes.Create(4);

    // Configure Wi-Fi for mesh communication
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Assign static positions in a 2x2 grid layout
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack and assign IP addresses
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP Echo Servers on all nodes
    uint16_t port = 9;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(i));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(15.0));
    }

    // Set up UDP Echo Clients on all nodes to communicate with others
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        for (uint32_t j = 0; j < nodes.GetN(); ++j) {
            if (i != j) {
                UdpEchoClientHelper echoClient(interfaces.GetAddress(j), port);
                echoClient.SetAttribute("MaxPackets", UintegerValue(15));
                echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
                echoClient.SetAttribute("PacketSize", UintegerValue(512));

                ApplicationContainer clientApp = echoClient.Install(nodes.Get(i));
                clientApp.Start(Seconds(2.0));
                clientApp.Stop(Seconds(15.0));
            }
        }
    }

    // Enable global routing for connectivity
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable PCAP tracing
    phy.EnablePcapAll("mesh_topology_udp_simulation");

    // Run the simulation
    Simulator::Stop(Seconds(15.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}