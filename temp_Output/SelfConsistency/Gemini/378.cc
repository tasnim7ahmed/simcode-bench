#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/internet-module.h"
#include "ns3/random-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MeshExample");

int main (int argc, char *argv[])
{
  // Enable logging
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer meshNodes;
  meshNodes.Create (3);

  // Set up Wifi PHY
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Configure mesh MAC
  MeshHelper mesh;
  mesh.SetStandard (WIFI_PHY_STANDARD_80211s);
  mesh.SetMacType ("RandomAccessMeshMac");

  // Install mesh on nodes
  NetDeviceContainer meshDevices = mesh.Install (wifiPhy, meshNodes);

  // Install TCP/IP stack
  InternetStackHelper internet;
  internet.Install (meshNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

  // Create UDP server on node 2 (index 1)
  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (meshNodes.Get (2)); // Corrected index
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Create UDP client on node 1 (index 0), sending to node 3 (index 2)
  UdpClientHelper client (interfaces.GetAddress (2), 9); // Corrected address index and port
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (meshNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));


  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}