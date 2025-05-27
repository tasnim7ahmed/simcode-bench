#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpEcho");

int main (int argc, char *argv[])
{
  bool ipv4 = true;
  bool ipv6 = true;

  CommandLine cmd;
  cmd.AddValue ("ipv4", "Enable IPv4", ipv4);
  cmd.AddValue ("ipv6", "Enable IPv6", ipv6);
  cmd.Parse (argc, argv);

  if (!ipv4 && !ipv6)
    {
      std::cerr << "At least one of IPv4 or IPv6 must be enabled" << std::endl;
      return 1;
    }

  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("CsmaNetDevice", LOG_LEVEL_ALL);
  LogComponentEnable ("QueueDisc", LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create (4);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1400));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  if (ipv4)
    {
      interfaces = address.Assign (devices);
      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    }

  Ipv6AddressHelper address6;
  address6.SetBase ("2001:db8:0:0:0:0:0:0", Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces6;
  if (ipv6)
    {
      interfaces6 = address6.Assign (devices);
      Ipv6GlobalRoutingHelper::PopulateRoutingTables ();
    }

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (ipv6)
    {
      UdpEchoClientHelper echoClient6 (interfaces6.GetAddress (1), 9);
      echoClient6.SetAttribute ("MaxPackets", UintegerValue (10));
      echoClient6.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
      echoClient6.SetAttribute ("PacketSize", UintegerValue (1024));

      ApplicationContainer clientApps6 = echoClient6.Install (nodes.Get (0));
      clientApps6.Start (Seconds (2.0));
      clientApps6.Stop (Seconds (10.0));
    }

  Simulator::Stop (Seconds (11.0));

  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("udp-echo.tr"));
  csma.EnablePcapAll ("udp-echo", true);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}