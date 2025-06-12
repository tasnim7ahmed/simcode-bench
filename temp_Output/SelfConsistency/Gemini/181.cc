#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-mac-module.h"
#include "ns3/wave-net-device.h"
#include "ns3/wave-helper.h"
#include "ns3/oinfo-module.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/propagation-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetPrintLevel (LogComponent::Level::LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  // Mobility model
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

  // Set initial velocities
  Ptr<ConstantVelocityMobilityModel> mob0 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  mob0->SetVelocity(Vector(10, 0, 0)); // 10 m/s along X axis

  Ptr<ConstantVelocityMobilityModel> mob1 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
  mob1->SetVelocity(Vector(10, 0, 0)); // 10 m/s along X axis

  // WAVE configuration
  WaveMacHelper waveMacHelper;
  WaveHelper waveHelper;
  waveHelper.SetRemoteStationManager ("ns3::ConstantRateWifiMacManager",
                                      "DataTxMode",StringValue("OfdmRate6MbpsBW10MHz"),
                                      "ControlTxMode",StringValue("OfdmRate6MbpsBW10MHz"));

  NetDeviceContainer devices = waveHelper.Install (waveMacHelper, nodes);

  // Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  // UDP Echo application
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (i.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));


  // Animation
  AnimationInterface anim ("vanet_simulation.xml");
  anim.SetConstantPosition (nodes.Get (0), 0.0, 0.0);
  anim.SetConstantPosition (nodes.Get (1), 5.0, 0.0);

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}