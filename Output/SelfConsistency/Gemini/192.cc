#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/constant-position-mobility-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleWireless");

int
main (int argc, char *argv[])
{
  LogComponent::SetLogLevel (LogComponent::GetComponent ("PacketSink"), LOG_LEVEL_INFO);
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices1 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices2 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign (devices1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign (devices2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Configure CBR traffic source
  uint16_t port = 9; // Discard port (RFC 793)
  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (interfaces2.GetAddress (1), port)));
  onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("PacketSize", UintegerValue (1024));
  onoff.SetAttribute ("DataRate", StringValue ("1Mbps"));
  ApplicationContainer apps = onoff.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  // Configure packet sink
  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  apps = sink.Install (nodes.Get (2));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  // Tracing
  pointToPoint.EnablePcapAll ("simple-wireless");

  // Set up positions for NetAnim
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  Ptr<ConstantPositionMobilityModel> mob0 = nodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  Ptr<ConstantPositionMobilityModel> mob1 = nodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
  Ptr<ConstantPositionMobilityModel> mob2 = nodes.Get(2)->GetObject<ConstantPositionMobilityModel>();

  mob0->SetPosition(Vector(0.0, 0.0, 0.0));
  mob1->SetPosition(Vector(50.0, 0.0, 0.0));
  mob2->SetPosition(Vector(100.0, 0.0, 0.0));

  AnimationInterface anim ("simple-wireless.xml");
  anim.SetConstantPosition (nodes.Get (0), 0, 0);
  anim.SetConstantPosition (nodes.Get (1), 50, 0);
  anim.SetConstantPosition (nodes.Get (2), 100, 0);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}