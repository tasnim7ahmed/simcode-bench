#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mesh-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/vector.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MeshExample");

int
main (int argc, char *argv[])
{
  // Enable logging from the MeshHelper
  LogComponentEnable ("MeshExample", LOG_LEVEL_INFO);
  LogComponentEnable ("MeshHelper", LOG_LEVEL_ALL);
  LogComponentEnable ("AdhocWifiMac", LOG_LEVEL_ALL);

  // Create mesh helper object
  MeshHelper mesh;
  // Set default parameters for mesh networks
  mesh.SetStandard (WIFI_PHY_STANDARD_80211_G);
  mesh.SetOperatingMode (MeshHelper::OCB);
  mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);

  // Create mesh nodes
  NodeContainer meshNodes;
  meshNodes.Create (3);

  // Install wireless devices on nodes
  mesh.Install (meshNodes);

  // Fix nodes position (grid layout)
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (meshNodes);

  // Install TCP/IP stack
  InternetStackHelper internet;
  internet.Install (meshNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (meshNodes);

  // Create UDP echo server on the last node
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (meshNodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Create UDP echo client on the first node
  UdpEchoClientHelper echoClient (interfaces.GetAddress (2), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer clientApps = echoClient.Install (meshNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}