#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/propagation-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include <iostream>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VANETSimulation");

int main (int argc, char *argv[])
{
  bool enableFlowMonitor = false;

  CommandLine cmd;
  cmd.AddValue ("EnableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (5);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("100.0"),
                                 "Y", StringValue ("100.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=30.0]"));
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes);

  for (uint32_t i = 0; i < nodes.GetN (); ++i)
  {
    Ptr<ConstantVelocityMobilityModel> mob = nodes.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
    mob->SetVelocity (Vector (10, 0, 0)); // Move along X axis at 10 m/s
  }

  // Wifi configuration
  Wifi80211pHelper wifi80211pHelper;
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMacHelper;
  Ssid ssid = Ssid ("vanet-ssid");
  wifiMacHelper.SetType ("ns3::AdhocWifiMac",
                         "Ssid", SsidValue (ssid));

  NetDeviceContainer devices = wifi80211pHelper.Install (wifiPhy, wifiMacHelper, nodes);

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.10.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Application (BSM - Basic Safety Message)
  uint16_t port = 9; // Standard BSM port
  UdpClientHelper client (interfaces.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (100)));
  client.SetAttribute ("PacketSize", UintegerValue (100));

  ApplicationContainer clientApps = client.Install (nodes.Get (1)); // Send from node 1
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (0)); // Receive at node 0
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // NetAnim
  AnimationInterface anim ("vanet-animation.xml");
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
  {
    anim.UpdateNodeColor (nodes.Get (i), 0, 255, 0);
  }

  // FlowMonitor
  FlowMonitorHelper flowmon;
  if (enableFlowMonitor)
  {
    flowmon.InstallAll ();
  }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  if (enableFlowMonitor)
  {
    flowmon.SerializeToXmlFile("vanet-flowmonitor.xml", false, true);
  }

  Simulator::Destroy ();
  return 0;
}