#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/mesh-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    Config::SetDefault ("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue (11472));
    CommandLine cmd;
    cmd.Parse (argc, argv);

    NodeContainer meshNodes;
    meshNodes.Create (5);

    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211s);

    MeshHelper mesh;
    mesh.SetStackInstaller ("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType ("RandomStart");
    NetDeviceContainer meshDevices = mesh.Install (wifi, meshNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                     "X", StringValue ("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"),
                                     "Y", StringValue ("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"),
                                     "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
    mobility.Install (meshNodes);

    InternetStackHelper internet;
    internet.Install (meshNodes);

    Ipv4AddressHelper address;
    address.SetBase ("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

    UdpEchoServerHelper echoServer (9);
    ApplicationContainer serverApps = echoServer.Install (meshNodes.Get (4));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    UdpEchoClientHelper echoClient (interfaces.GetAddress (4), 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (3));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (512));

    ApplicationContainer clientApps = echoClient.Install (meshNodes.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}