#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-client.h"
#include "ns3/udp-echo-server.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMeshExample");

int main (int argc, char *argv[])
{
  // Enable logging
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer meshNodes;
  meshNodes.Create (5);

  // Configure Wi-Fi mesh
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211s);

  MeshHelper mesh;
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  mesh.SetSpreadInterfaceChannels (true);
  mesh.SetMacType ("RandomStart");

  NetDeviceContainer meshDevices = mesh.Install (wifi, meshNodes);

  // Configure mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (meshNodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (meshNodes);

  // Assign IPv4 addresses
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

  // Install UDP echo server on the last node
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (meshNodes.Get (4));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Install UDP echo client on the first node
  UdpEchoClientHelper echoClient (interfaces.GetAddress (4), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (3));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (512));

  ApplicationContainer clientApps = echoClient.Install (meshNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Run the simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}