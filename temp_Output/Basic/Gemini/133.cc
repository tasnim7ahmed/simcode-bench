#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/rip.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SplitHorizonSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel(LogLevel::LOG_LEVEL_INFO);
  LogComponent::SetDefaultLogComponentEnable(true);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devicesAB = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devicesBC = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAB = address.Assign (devicesAB);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesBC = address.Assign (devicesBC);

  Rip rip;
  rip.SetSplitHorizon (true);

  Ipv4GlobalRoutingHelper globalRouting;
  globalRouting.SetRoutingProtocol ("ns3::Rip", "SplitHorizon", BooleanValue (true));

  Address sinkAddress (InetSocketAddress (interfacesBC.GetAddress (1), 80));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (2));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  uint32_t packetSize = 1024;
  uint32_t maxPackets = 1000;
  Time interPacketInterval = Seconds (0.01);
  TcpEchoClientHelper clientHelper (interfacesBC.GetAddress (1), 80);
  clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  clientHelper.SetAttribute ("InterPacketInterval", TimeValue (interPacketInterval));
  ApplicationContainer clientApps = clientHelper.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}