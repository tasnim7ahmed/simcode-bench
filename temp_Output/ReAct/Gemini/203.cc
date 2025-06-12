#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/propagation-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("V2V");

int main (int argc, char *argv[])
{
  bool enableFlowMonitor = true;

  CommandLine cmd;
  cmd.AddValue ("EnableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (5);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211p);

  NqosWaveMacHelper mac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p;

  NetDeviceContainer devices = wifi80211p.Install (phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (20.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes);

  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<ConstantVelocityMobilityModel> mob = nodes.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      mob->SetVelocity (Vector (10 + i, 0, 0));
    }

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < nodes.GetN (); ++i)
    {
      clientApps.Add (echoClient.Install (nodes.Get (i)));
    }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  AnimationInterface anim ("vanet.xml");
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      anim.UpdateNodeColor (nodes.Get (i), 0, 0, 255);
    }

  FlowMonitorHelper flowmon;
  if (enableFlowMonitor)
    {
      flowmon.InstallAll ();
    }

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();

  if (enableFlowMonitor)
    {
      flowmon.SerializeToXmlFile ("vanet.flowmon", false, true);
    }

  Simulator::Destroy ();
  return 0;
}