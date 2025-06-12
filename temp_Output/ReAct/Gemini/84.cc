#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaTraceExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool useIpv6 = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("useIpv6", "Use IPv6 if true", useIpv6);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetChannelAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));

  InterfaceContainer interfaces;
  if (useIpv6)
    {
      interfaces = ipv6.Assign (devices);
      for (uint32_t i = 0; i < interfaces.GetN (); ++i)
        {
          interfaces.GetAddress (i, 1);
        }
    }
  else
    {
      interfaces = address.Assign (devices);
      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    }

  uint16_t port = 9;  // Discard port (RFC 863)

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  Ptr<TraceSourceAccessor> traceSource = CreateObject<TraceSourceAccessor> ("filename");
  UdpClientHelper client (useIpv6 ? interfaces.GetAddress (1, 1) : interfaces.GetAddress (1, 0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  client.SetAttribute ("TraceSource", PointerValue (traceSource));
  client.SetAttribute ("filename", StringValue ("trace.txt"));
  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (tracing)
    {
      csma.EnablePcap ("csma-trace", devices, true);
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}