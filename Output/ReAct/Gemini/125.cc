#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/rip.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipStar");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel (Rip::TypeId (), LOG_LEVEL_ALL);
  LogComponent::SetPrintMask (LOG_PREFIX_TIME | LOG_PREFIX_NODE);

  NodeContainer nodes;
  nodes.Create (5);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices[4];
  Ipv4InterfaceContainer interfaces[4];

  for (int i = 0; i < 4; ++i)
    {
      NetDeviceContainer tempDevices = pointToPoint.Install (nodes.Get (0), nodes.Get (i + 1));
      devices[i] = tempDevices;
    }

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  for (int i = 0; i < 4; ++i)
    {
      std::stringstream subnetAddress;
      subnetAddress << "10.1." << i + 1 << ".0";
      address.SetBase (subnetAddress.str ().c_str (), "255.255.255.0");
      Ipv4InterfaceContainer tempInterfaces = address.Assign (devices[i]);
      interfaces[i] = tempInterfaces;
    }

  Rip rip;
  rip.ExcludeInterface (nodes.Get (0)->GetObject<Ipv4>()->GetAddress (1,0).GetLocal ());
  rip.ExcludeInterface (nodes.Get (1)->GetObject<Ipv4>()->GetAddress (1,0).GetLocal ());
  rip.ExcludeInterface (nodes.Get (2)->GetObject<Ipv4>()->GetAddress (1,0).GetLocal ());
  rip.ExcludeInterface (nodes.Get (3)->GetObject<Ipv4>()->GetAddress (1,0).GetLocal ());
  rip.ExcludeInterface (nodes.Get (4)->GetObject<Ipv4>()->GetAddress (1,0).GetLocal ());

  RipHelper ripRouting;
  ripRouting.SetInterfaceMetric (devices[0].Get (0), 1);
  ripRouting.SetInterfaceMetric (devices[1].Get (0), 1);
  ripRouting.SetInterfaceMetric (devices[2].Get (0), 1);
  ripRouting.SetInterfaceMetric (devices[3].Get (0), 1);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("rip-routing.routes", std::ios::out);
  Simulator::Schedule (Seconds (10.0), &Ipv4::PrintRoutingTableAllAt, routingStream, 0);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (4));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces[3].GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}