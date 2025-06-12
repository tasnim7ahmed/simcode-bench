#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpEchoSimulation");

int main (int argc, char *argv[])
{
  bool useIpv6 = false;
  CommandLine cmd (__FILE__);
  cmd.AddValue ("useIpv6", "Use IPv6 if true, IPv4 otherwise", useIpv6);
  cmd.Parse (argc, argv);

  NS_LOG_INFO ("Creating nodes.");
  NodeContainer nodes;
  nodes.Create (4);

  NodeContainer csmaNodes = nodes;

  NS_LOG_INFO ("Creating CSMA channel.");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (5000000)));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1400));

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  NS_LOG_INFO ("Installing internet stack.");
  InternetStackHelper internet;
  internet.Install (nodes);

  NS_LOG_INFO ("Assigning IP addresses.");
  Ipv4AddressHelper ipv4;
  Ipv6AddressHelper ipv6;
  Ipv4InterfaceContainer csmaInterfaces;
  Ipv6InterfaceContainer csmaInterfaces6;

  if (!useIpv6)
    {
      ipv4.SetBase ("10.1.0.0", "255.255.255.0");
      csmaInterfaces = ipv4.Assign (csmaDevices);
    }
  else
    {
      ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
      csmaInterfaces6 = ipv6.AssignWithoutRouter (csmaDevices);
    }

  NS_LOG_INFO ("Creating UDP Echo Server on node n1.");
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (11.0));

  NS_LOG_INFO ("Creating UDP Echo Client on node n0.");
  UdpEchoClientHelper echoClient (useIpv6 ? csmaInterfaces6.GetAddress (1, 1) : csmaInterfaces.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (9));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("udp-echo.tr");
  csma.EnableAsciiAll (stream);
  csma.EnablePcapAll ("udp-echo");

  NS_LOG_INFO ("Starting simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}