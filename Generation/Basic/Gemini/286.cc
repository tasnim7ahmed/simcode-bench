#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create 5 nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Configure mobility for nodes (RandomWaypointMobilityModel)
    MobilityHelper mobility;
    
    // Define the movement area (0 to 100 in X and Y)
    Ptr<RandomRectanglePositionAllocator> positionAlloc =
        CreateObject<RandomRectanglePositionAllocator>();
    positionAlloc->SetAttribute("X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    positionAlloc->SetAttribute("Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=5.0]"), // 5 m/s speed
                              "PauseTime", StringValue("ns3::UniformRandomVariable[Min=2.0|Max=2.0]"), // 2 seconds pause
                              "PositionAllocator", PointerValue(positionAlloc));
    mobility.Install(nodes);

    // Configure WiFi (802.11b, Adhoc mode)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    WifiPhyHelper wifiPhy;
    // Set up a default YansWifiChannel with Friis and ConstantSpeed models
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLoss("ns3::FriisPropagationLossModel");
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Install AODV routing protocol
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses to devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup UDP Server on Node 4
    Ptr<Node> serverNode = nodes.Get(4);
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(serverNode);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // Setup UDP Client on Node 0
    Ptr<Node> clientNode = nodes.Get(0);
    // Get the IP address of Node 4 (which is at index 4 in the interfaces container)
    Ipv4Address serverAddress = interfaces.GetAddress(4); 
    
    UdpClientHelper client(serverAddress, port);
    client.SetAttribute("MaxPackets", UintegerValue(100000)); // Large enough to send throughout simulation
    client.SetAttribute("Interval", TimeValue(MilliSeconds(50))); // Send every 50ms
    client.SetAttribute("PacketSize", UintegerValue(512)); // 512-byte packets
    ApplicationContainer clientApps = client.Install(clientNode);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(20.0));

    // Enable PCAP tracing for all WiFi devices
    wifiPhy.EnablePcapAll("manet-aodv-trace");

    // Set simulation duration
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}