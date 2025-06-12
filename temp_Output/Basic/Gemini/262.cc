#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMeshExample");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  uint32_t packetSize = 512;
  uint32_t numPackets = 3;
  double interval = 1.0;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable pcap tracing", enablePcap);
  cmd.AddValue ("packetSize", "Size of packets to send", packetSize);
  cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue ("interval", "Interval between packets", interval);
  cmd.Parse (argc, argv);

  NodeContainer meshNodes;
  meshNodes.Create (5);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211_S);

  MeshHelper mesh;
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  mesh.SetSpreadInterfaceChannels (true);

  mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.01)));

  NetDeviceContainer meshDevices = mesh.Install (wifi, meshNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
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
  echoClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps = echoClient.Install (meshNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  if (enablePcap)
  {
    wifi.EnablePcapAll ("mesh-wifi", false);
  }

  AnimationInterface anim ("mesh-animation.xml");
  anim.SetConstantPosition (meshNodes.Get(0), 0.0, 0.0);
  anim.SetConstantPosition (meshNodes.Get(1), 20.0, 0.0);
  anim.SetConstantPosition (meshNodes.Get(2), 0.0, 20.0);
  anim.SetConstantPosition (meshNodes.Get(3), 20.0, 20.0);
  anim.SetConstantPosition (meshNodes.Get(4), 10.0, 10.0);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}