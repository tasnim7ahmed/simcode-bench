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

static void
InstallStaticRoute (Ptr<Node> node, Ipv4Address gateway)
{
  Ptr<Ipv4StaticRouting> routing = node->GetObject<Ipv4> ()->GetRoutingProtocol ()->GetObject<Ipv4StaticRouting> ();
  routing->SetDefaultRoute (gateway, 1);
}


int main (int argc, char *argv[])
{
  bool enableAsciiTrace = false;
  bool enableNetAnim = false;

  CommandLine cmd;
  cmd.AddValue ("EnableAsciiTrace", "Enable Ascii trace", enableAsciiTrace);
  cmd.AddValue ("EnableNetAnim", "Enable NetAnim", enableNetAnim);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ("UdpClient", LOG_LEVEL_INFO);
  LogComponent::SetFilter ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211s);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  MeshHelper mesh;
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  mesh.SetMacType ("ns3::AdhocWifiMac", "Ssid", SsidValue (Ssid ("adhoc-mesh")));

  NetsDeviceContainer devices = mesh.Install (phy, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes.Get (0));

  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes.Get (1));

  Ptr<ConstantVelocityMobilityModel> mobModel = nodes.Get (1)->GetObject<ConstantVelocityMobilityModel> ();
  mobModel->SetVelocity (Vector (10, 0, 0)); // Move right at 10 m/s


  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper echoClient (interfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  if (enableAsciiTrace)
    {
      AsciiTraceHelper ascii;
      phy.EnableAsciiAll (ascii.CreateFileStream ("adhoc-wifi-mesh.tr"));
    }

  if(enableNetAnim){
      AnimationInterface anim ("adhoc-wifi-mesh.xml");
      anim.SetConstantPosition (nodes.Get (0), 0.0, 0.0);
      anim.SetConstantPosition (nodes.Get (1), 0.0, 10.0);
  }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}