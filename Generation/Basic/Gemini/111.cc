#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/radvd-helper.h"
#include "ns3/ping6-helper.h"
#include "ns3/ascii-trace-helper.h"
#include "ns3/ipv6-l3-protocol.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  LogComponentEnable ("Ping6Application", LOG_LEVEL_INFO);
  LogComponentEnable ("RadvdApplication", LOG_LEVEL_INFO);

  Time::SetResolution (Time::NS);

  // Create nodes: n0, n1 (end hosts), and R (router)
  NodeContainer n0, n1, R;
  n0.Create (1);
  n1.Create (1);
  R.Create (1);

  // Install Internet Stack (includes IPv6) on all nodes
  InternetStackHelper internetStackHelper;
  internetStackHelper.Install (n0);
  internetStackHelper.Install (n1);
  internetStackHelper.Install (R);

  // Create CSMA channels and devices
  CsmaHelper csmaHelper0R;
  csmaHelper0R.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csmaHelper0R.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer devices0R = csmaHelper0R.Install (NodeContainer (n0, R)); // n0.Get(0) is n0's device, R.Get(0) is R's device

  CsmaHelper csmaHelper1R;
  csmaHelper1R.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csmaHelper1R.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer devices1R = csmaHelper1R.Install (NodeContainer (n1, R)); // n1.Get(0) is n1's device, R.Get(0) is R's device

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;

  // Configure addresses for n0-R link (2001:1::/64)
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces0R = ipv6.Assign (devices0R);
  // Set R's interface on this link as forwarding
  interfaces0R.SetForwarding (1, true); // devices0R.Get(1) is R's device
  // Set n0's interface as not forwarding (end host)
  interfaces0R.SetForwarding (0, false); // devices0R.Get(0) is n0's device

  // Configure addresses for n1-R link (2001:2::/64)
  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces1R = ipv6.Assign (devices1R);
  // Set R's interface on this link as forwarding
  interfaces1R.SetForwarding (1, true); // devices1R.Get(1) is R's device
  // Set n1's interface as not forwarding (end host)
  interfaces1R.SetForwarding (0, false); // devices1R.Get(0) is n1's device

  // Configure Radvd on the router (R)
  // Get the IPv6L3Protocol object from the router node to find interface indices
  Ptr<Ipv6L3Protocol> ipv6L3R = R.Get (0)->GetObject<Ipv6L3Protocol> ();

  // Get the internal interface indices for the router's CSMA devices
  uint32_t routerIf0Index = ipv6L3R->GetInterfaceForDevice (devices0R.Get (1)); // R's device on n0-R link
  uint32_t routerIf1Index = ipv6L3R->GetInterfaceForDevice (devices1R.Get (1)); // R's device on n1-R link

  RadvdHelper radvdHelper;
  ApplicationContainer radvdApps = radvdHelper.Install (R.Get (0)); // Install RadvdApplication on the router

  // Get the pointer to the installed RadvdApplication
  Ptr<RadvdApplication> radvdApp = DynamicCast<RadvdApplication> (radvdApps.Get (0));

  // Add prefixes to be advertised for each subnet
  radvdApp->AddPrefix (routerIf0Index, Ipv6Prefix ("2001:1::/64"));
  radvdApp->AddPrefix (routerIf1Index, Ipv6Prefix ("2001:2::/64"));

  radvdApps.Start (Seconds (0.5)); // Start Router Advertisements early
  radvdApps.Stop (Seconds (10.0)); // Continue for most of the simulation

  // Configure Ping6 application from n0 to n1
  // Get n1's global IPv6 address from the interfaces1R container (n1 is at index 0)
  Ipv6Address n1GlobalAddress = interfaces1R.GetAddress (0).GetAddress ();

  Ping6Helper ping6;
  ping6.SetRemote (n1GlobalAddress);
  ping6.SetAttribute ("Count", UintegerValue (4));        // Send 4 echo requests
  ping6.SetAttribute ("Interval", TimeValue (Seconds (1.0))); // 1 second interval
  ping6.SetAttribute ("Verbose", BooleanValue (true));    // Print detailed ping output
  ApplicationContainer pingApps = ping6.Install (n0.Get (0)); // Install Ping6 on n0

  pingApps.Start (Seconds (2.0)); // Start ping after RAs have had a chance to be sent
  pingApps.Stop (Seconds (6.0));  // Stop after 4 pings + interval (2 + 4*1 = 6s)

  // Enable tracing of queues and packet receptions
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("radvd.tr");

  // Trace all devices on both CSMA channels
  csmaHelper0R.EnableAsciiTrace (stream, devices0R.Get (0)); // n0's device
  csmaHelper0R.EnableAsciiTrace (stream, devices0R.Get (1)); // R's device on n0-R link
  csmaHelper1R.EnableAsciiTrace (stream, devices1R.Get (0)); // n1's device
  csmaHelper1R.EnableAsciiTrace (stream, devices1R.Get (1)); // R's device on n1-R link

  // Populate IPv6 global routing tables, essential for router to forward between subnets
  Ipv6GlobalRoutingHelper::PopulateRoutingTables ();

  // Set simulation stop time and run
  Simulator::Stop (Seconds (7.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}