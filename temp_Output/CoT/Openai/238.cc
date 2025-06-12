#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocMeshMobileUdpExample");

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);

  uint32_t packetSize = 1024;
  uint32_t numPackets = 50;
  double interval = 0.2;
  double simTime = 15.0;

  NodeContainer nodes;
  nodes.Create (2); // 0: mobile, 1: static

  // Set mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // Mobile node starts here
  positionAlloc->Add (Vector (100.0, 0.0, 0.0)); // Static node
  mobility.SetPositionAllocator (positionAlloc);

  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes);
  nodes.Get (0)->GetObject<ConstantVelocityMobilityModel> ()->SetVelocity (Vector (6.0, 0.0, 0.0)); // Move along X axis
  nodes.Get (1)->GetObject<MobilityModel> ()->SetPosition (Vector (100.0, 0.0, 0.0)); // Static

  // Wi-Fi 802.11s Mesh
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  MeshHelper mesh;
  mesh = MeshHelper::Default ();
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
  mesh.SetNumberOfInterfaces (1);

  NetDeviceContainer meshDevices = mesh.Install (wifiPhy, nodes);

  // Internet stack
  InternetStackHelper internetStack;
  internetStack.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (meshDevices);

  // UDP Server on node 1 (static)
  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simTime));

  // UDP Client on node 0 (mobile)
  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simTime));

  // Tracing
  wifiPhy.EnablePcap ("adhoc-mesh-mobile", meshDevices);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}