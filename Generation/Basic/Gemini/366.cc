#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int main ()
{
  ns3::Config::SetDefault ("ns3::TcpL4Protocol::SocketType", ns3::StringValue ("ns3::TcpNewReno"));

  ns3::NodeContainer nodes;
  nodes.Create (2);

  ns3::PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", ns3::StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", ns3::StringValue ("2ms"));

  ns3::NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  ns3::InternetStackHelper internet;
  internet.Install (nodes);

  ns3::Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  uint16_t port = 9;

  // Server (Node 0)
  ns3::Address sinkAddress (ns3::InetSocketAddress (ns3::Ipv4Address::GetAny (), port));
  ns3::PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ns3::ApplicationContainer serverApps = packetSinkHelper.Install (nodes.Get (0));
  serverApps.Start (ns3::Seconds (0.0));
  serverApps.Stop (ns3::Seconds (10.0));

  // Client (Node 1)
  ns3::OnOffHelper onoffHelper ("ns3::TcpSocketFactory", ns3::Address ());
  onoffHelper.SetAttribute ("OnTime", ns3::StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoffHelper.SetAttribute ("OffTime", ns3::StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoffHelper.SetAttribute ("PacketSize", ns3::UintegerValue (1024));
  onoffHelper.SetAttribute ("DataRate", ns3::StringValue ("100Mbps"));

  ns3::AddressValue remoteAddress (ns3::InetSocketAddress (interfaces.GetAddress (0), port));
  onoffHelper.SetAttribute ("Remote", remoteAddress);

  ns3::ApplicationContainer clientApps = onoffHelper.Install (nodes.Get (1));
  clientApps.Start (ns3::Seconds (1.0));
  clientApps.Stop (ns3::Seconds (10.0));

  ns3::Simulator::Stop (ns3::Seconds (10.0));
  ns3::Simulator::Run ();
  ns3::Simulator::Destroy ();

  return 0;
}