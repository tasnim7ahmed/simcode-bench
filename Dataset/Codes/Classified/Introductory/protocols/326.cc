#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetExample");

int main(int argc, char *argv[])
{
    // Enable log components
    LogComponentEnable("ManetExample", LOG_LEVEL_INFO);

    // Create nodes for the MANET
    NodeContainer nodes;
    nodes.Create(5);

    // Set up mobility model for nodes (random movement)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "MinX", DoubleValue(0.0),
                              "MinY", DoubleValue(0.0),
                              "MaxX", DoubleValue(100.0),
                              "MaxY", DoubleValue(100.0),
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=10.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.Install(nodes);

    // Install AODV routing protocol
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv); // Use AODV as the routing protocol
    stack.Install(nodes);

    // Set up point-to-point devices (Wi-Fi or similar)
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211b);
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-manet");
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices = wifiHelper.Install(wifiPhy, wifiMac, nodes);

    // Install the internet stack and assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ipInterfaces = ipv4.Assign(devices);

    // Set up application on node 0 (sending data)
    OnOffHelper onOffHelper("ns3::UdpSocketFactory", Address(InetSocketAddress(ipInterfaces.GetAddress(4), 9))); // Send to node 4
    onOffHelper.SetConstantRate(DataRate("500kb/s"));
    ApplicationContainer senderApp = onOffHelper.Install(nodes.Get(0));
    senderApp.Start(Seconds(2.0));
    senderApp.Stop(Seconds(10.0));

    // Set up application on node 4 (receiving data)
    UdpServerHelper udpServer(9);
    ApplicationContainer receiverApp = udpServer.Install(nodes.Get(4));
    receiverApp.Start(Seconds(2.0));
    receiverApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
