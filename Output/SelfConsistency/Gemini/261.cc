#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-net-device.h"
#include "ns3/wave-helper.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-application.h"
#include "ns3/udp-echo-server.h"
#include "ns3/udp-echo-client.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (3);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes);

  Ptr<ConstantVelocityMobilityModel> mob0 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  mob0->SetVelocity(Vector(20, 0, 0)); // 20 m/s
  Ptr<ConstantVelocityMobilityModel> mob1 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
  mob1->SetVelocity(Vector(15, 0, 0)); // 15 m/s
  Ptr<ConstantVelocityMobilityModel> mob2 = nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>();
  mob2->SetVelocity(Vector(25, 0, 0)); // 25 m/s

  // Wifi (802.11p)
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WaveMacHelper wifi80211pMac = WaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
  NetDeviceContainer devices = wifi80211p.Install (wifiPhy, wifi80211pMac, nodes);

  // Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Applications
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (2), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));


  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}