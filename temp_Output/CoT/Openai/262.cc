#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
    // Set defaults for Wi-Fi
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Set up mobility: RandomWalk2dMobilityModel within (-50,50,-50,50)
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue ("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"),
                                  "Y", StringValue ("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"));
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", BoxValue(Box(-50.0, 50.0, -50.0, 50.0, 0.0, 0.0)));
    mobility.Install(nodes);

    // Set up Wi-Fi mesh
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel (wifiChannel.Create ());
    WifiHelper wifi = WifiHelper::Default ();
    wifi.SetStandard (WIFI_STANDARD_80211s);

    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType ("RandomStart", TimeValue (Seconds(0.1)));
    mesh.SetNumberOfInterfaces (1);

    NetDeviceContainer meshDevices = mesh.Install (wifiPhy, nodes);

    // Internet stack and IP addressing
    InternetStackHelper internetStack;
    internetStack.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

    // UDP Echo server on the last node (node 4), port 9
    uint16_t port = 9;
    UdpEchoServerHelper echoServer (port);
    ApplicationContainer serverApps = echoServer.Install (nodes.Get(4));
    serverApps.Start (Seconds(1.0));
    serverApps.Stop (Seconds(10.0));

    // UDP Echo client on the first node (node 0)
    UdpEchoClientHelper echoClient (interfaces.GetAddress(4), port);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (3));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (512));

    ApplicationContainer clientApps = echoClient.Install (nodes.Get(0));
    clientApps.Start (Seconds(2.0));
    clientApps.Stop (Seconds(10.0));

    // Enable logs and tracing if desired (disabled by default for minimal output)
    // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    Simulator::Stop (Seconds(10.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}