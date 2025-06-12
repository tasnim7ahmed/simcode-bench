#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FragmentationIPv6PMTU");

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("Ipv6Interface", LOG_LEVEL_ALL);

  NodeContainer src;
  src.Create (1);
  NodeContainer n0;
  n0.Create (1);
  NodeContainer r1;
  r1.Create (1);
  NodeContainer n1;
  n1.Create (1);
  NodeContainer r2;
  r2.Create (1);
  NodeContainer n2;
  n2.Create (1);

  NodeContainer p2pNodes1 = NodeContainer (src.Get (0), n0.Get (0));
  NodeContainer csmaNodes1 = NodeContainer (n0.Get (0), r1.Get (0));
  NodeContainer csmaNodes2 = NodeContainer (r1.Get (0), n1.Get (0));
  NodeContainer csmaNodes3 = NodeContainer (n1.Get (0), r2.Get (0));
  NodeContainer p2pNodes2 = NodeContainer (r2.Get (0), n2.Get (0));

  PointToPointHelper p2p1;
  p2p1.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p1.SetChannelAttribute ("Delay", StringValue ("2ms"));

  CsmaHelper csma1;
  csma1.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csma1.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p2.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices1 = p2p1.Install (p2pNodes1);
  NetDeviceContainer csmaDevices1 = csma1.Install (csmaNodes1);
  NetDeviceContainer csmaDevices2 = csma1.Install (csmaNodes2);
  NetDeviceContainer csmaDevices3 = csma1.Install (csmaNodes3);
  NetDeviceContainer p2pDevices2 = p2p2.Install (p2pNodes2);

  InternetStackHelper internetv6;
  internetv6.SetIpv4StackEnabled (false);
  internetv6.SetIpv6StackEnabled (true);
  internetv6.InstallAll ();

  Ipv6AddressHelper ipv6;

  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer p2pInterfaces1 = ipv6.Assign (p2pDevices1);
  p2pInterfaces1.SetForwarding (0, true);
  p2pInterfaces1.SetDefaultRouteInAllNodes (0);

  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer csmaInterfaces1 = ipv6.Assign (csmaDevices1);
  csmaInterfaces1.SetForwarding (0, true);
  csmaInterfaces1.SetForwarding (1, true);

  ipv6.SetBase (Ipv6Address ("2001:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer csmaInterfaces2 = ipv6.Assign (csmaDevices2);
  csmaInterfaces2.SetForwarding (0, true);
  csmaInterfaces2.SetForwarding (1, true);

  ipv6.SetBase (Ipv6Address ("2001:4::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer csmaInterfaces3 = ipv6.Assign (csmaDevices3);
  csmaInterfaces3.SetForwarding (0, true);
  csmaInterfaces3.SetForwarding (1, true);

  ipv6.SetBase (Ipv6Address ("2001:5::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer p2pInterfaces2 = ipv6.Assign (p2pDevices2);
  p2pInterfaces2.SetForwarding (1, true);
  p2pInterfaces2.SetDefaultRouteInAllNodes (1);

  Ipv6StaticRoutingHelper routingHelper;

  Ptr<Ipv6> ipv6Src = src.Get (0)->GetObject<Ipv6> ();
  Ptr<Ipv6StaticRouting> routingSrc = routingHelper.GetStaticRouting (ipv6Src);
  routingSrc->AddNetworkRouteTo (Ipv6Address ("2001:5::"), Ipv6Prefix (64), 1);

  Ptr<Ipv6> ipv6N2 = n2.Get (0)->GetObject<Ipv6> ();
  Ptr<Ipv6StaticRouting> routingN2 = routingHelper.GetStaticRouting (ipv6N2);
  routingN2->AddNetworkRouteTo (Ipv6Address ("2001:1::"), Ipv6Prefix (64), 1);

  Config::Set ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/Mtu", UintegerValue (5000));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/Mtu", UintegerValue (2000));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/Mtu", UintegerValue (1500));

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (n2.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (p2pInterfaces2.GetAddress (1, 1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (3000));

  ApplicationContainer clientApps = echoClient.Install (src.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("fragmentation-ipv6-PMTU.tr");
  p2pInterfaces1.EnableAsciiIpv6 (stream, p2pDevices1);
  csmaInterfaces1.EnableAsciiIpv6 (stream, csmaDevices1);
  csmaInterfaces2.EnableAsciiIpv6 (stream, csmaDevices2);
  csmaInterfaces3.EnableAsciiIpv6 (stream, csmaDevices3);
  p2pInterfaces2.EnableAsciiIpv6 (stream, p2pDevices2);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}