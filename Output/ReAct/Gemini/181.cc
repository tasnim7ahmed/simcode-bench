#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/nstime.h"
#include <iostream>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VANETSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  NqosWaveMacHelper mac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211pHelper = Wifi80211pHelper::Default ();

  NetDeviceContainer devices = wifi80211pHelper.Install (phy, mac, nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  UdpServerHelper server (9);
  ApplicationContainer apps = server.Install (nodes.Get (1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  UdpClientHelper client (i.GetAddress (1), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (100));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client.Install (nodes.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes);

  Ptr<ConstantVelocityMobilityModel> mob0 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  mob0->SetVelocity(Vector(10, 0, 0));

  Ptr<ConstantVelocityMobilityModel> mob1 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
  mob1->SetPosition(Vector(50,0,0));
  mob1->SetVelocity(Vector(10, 0, 0));

  AnimationInterface anim ("vanet.xml");
  anim.SetConstantPosition (nodes.Get(0), 0, 0);
  anim.SetConstantPosition (nodes.Get(1), 50, 0);

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}