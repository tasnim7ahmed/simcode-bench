#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/olsr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleManetExample");

int main(int argc, char *argv[])
{
    // Enable log components
    LogComponentEnable("SimpleManetExample", LOG_LEVEL_INFO);

    // Create mobile nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up Wi-Fi devices (STA and AP)
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211b);
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-manet");
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices = wifiHelper.Install(wifiPhy, wifiMac, nodes);

    // Set up internet stack (IP, UDP, etc.)
    InternetStackHelper stack;
    OlsrHelper olsr;
    stack.SetRoutingHelper(olsr);
    stack.Install(nodes);

    // Assign IP addresses to nodes
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ipInterfaces = ipv4.Assign(devices);

    // Set up UDP server on node 1
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on node 0
    UdpClientHelper udpClient(ipInterfaces.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set up random waypoint mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "MinX", DoubleValue(0.0),
                              "MaxX", DoubleValue(100.0),
                              "MinY", DoubleValue(0.0),
                              "MaxY", DoubleValue(100.0),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=20.0]"));
    mobility.Install(nodes);

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
