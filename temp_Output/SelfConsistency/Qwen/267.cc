#include "ns3/aodv-helper.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ns3-wifi-mac-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/position-allocator.h"
#include "ns3/simulator.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeNodeAodvSimulation");

int main(int argc, char *argv[]) {
    // Enable AODV logging
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_ALL);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Setup mobility - linear layout with 10m spacing
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));
    positionAlloc->Add(Vector(20.0, 0.0, 0.0));

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Setup Wi-Fi
    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(nodes);

    // Setup internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP echo server on node 2 (last node)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP client on node 0 (first node)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    wifi.EnablePcapAll("three-node-aodv");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}