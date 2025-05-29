#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  Time::SetResolution (Time::NS);

  // Create 3 mesh nodes
  NodeContainer meshNodes;
  meshNodes.Create(3);

  // Configure mesh helper for 802.11s
  MeshHelper mesh;
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
  mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
  mesh.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue ("HtMcs7"),
                               "ControlMode", StringValue ("HtMcs0"));

  // Install mesh devices
  NetDeviceContainer meshDevices = mesh.Install (WifiPhyHelper::Default (), meshNodes);

  // Arrange nodes in a grid, 5 meters apart
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
      "MinX", DoubleValue (0.0),
      "MinY", DoubleValue (0.0),
      "DeltaX", DoubleValue (5.0),
      "DeltaY", DoubleValue (0.0),
      "GridWidth", UintegerValue (3),
      "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (meshNodes);

  // Install Internet stack and assign IP addresses
  InternetStackHelper internetStack;
  internetStack.Install (meshNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

  // Set up UDP Echo Server on the last node (node 2), port 9
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install(meshNodes.Get(2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Set up UDP Echo Client on the first node (node 0)
  UdpEchoClientHelper echoClient (interfaces.GetAddress(2), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer clientApps = echoClient.Install(meshNodes.Get(0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}