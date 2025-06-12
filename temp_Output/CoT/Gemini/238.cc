#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocWifiMesh");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;
  double simulationTime = 10; // Seconds
  std::string phyMode ("OfdmRate6Mbps");
  uint32_t packetSize = 1024; // bytes
  uint32_t numPackets = 1000;
  double distance = 20.0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("time", "Simulation time (s)", simulationTime);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("distance", "distance between nodes", distance);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue (65535));

  NodeContainer nodes;
  nodes.Create (2);

  // The below set of helpers will help us create the mesh network.
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211s);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelay");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes.Get (0));
  mobility.Install (nodes.Get (1));

  Ptr<ConstantVelocityMobilityModel> mob = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  mob->SetVelocity(Vector3D(5, 0, 0));

  // Install Internet Stack
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  // UDP Echo Server on node 1
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  // UDP Echo Client on node 0
  UdpEchoClientHelper echoClient (i.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  // Tracing
  if (tracing)
    {
      phyPhy.EnablePcapAll("adhoc-wifi-mesh");
    }
   
  AnimationInterface anim("adhoc-wifi-mesh.xml");
  anim.SetConstantPosition(nodes.Get(1), 0.0, 10.0);

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}