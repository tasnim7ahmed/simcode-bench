#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetAodvExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("ManetAodvExample", LOG_LEVEL_INFO);

    // Create 5 mobile nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Configure WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Set up mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                              "PositionAllocator", StringValue("ns3::GridPositionAllocator"));
    mobility.Install(nodes);

    // Install Internet stack with AODV routing
    AodvHelper aodv;
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4ListRoutingHelper list;
    list.Add(staticRouting, 0);
    list.Add(aodv, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up a UDP server on Node 4
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(4));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Set up a UDP client on Node 0 sending to Node 4
    UdpClientHelper udpClient(interfaces.GetAddress(4), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    // Enable PCAP tracing
    wifiPhy.EnablePcap("manet-aodv", devices);

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
