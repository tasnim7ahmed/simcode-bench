#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RadvdTwoPrefixSimulation");

int main (int argc, char *argv[])
{
  bool verbose = true;
  uint32_t pingSize = 1024;
  uint32_t pingInterval = 1000; // ms
  uint32_t simDuration = 10;    // seconds

  CommandLine cmd (__FILE__);
  cmd.AddValue ("verbose", "Enable logging", verbose);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("RadvdTwoPrefixSimulation", LOG_LEVEL_INFO);
      LogComponentEnable ("Ipv6L3Protocol", LOG_LEVEL_INFO);
      LogComponentEnable ("Ping6Application", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  NodeContainer router;
  router.Create (1);

  NodeContainer net0 (nodes.Get (0), router.Get (0));
  NodeContainer net1 (nodes.Get (1), router.Get (0));

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (5000000)));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer devices0 = csma.Install (net0);
  NetDeviceContainer devices1 = csma.Install (net1);

  InternetStackHelper internetv6;
  internetv6.SetIpv4StackEnabled (false);
  internetv6.SetIpv6StackEnabled (true);
  internetv6.InstallAll ();

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces0 = ipv6.Assign (devices0);
  interfaces0.SetForwarding (1, true);
  interfaces0.SetDefaultRouteInAllNodes (1);

  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces1 = ipv6.Assign (devices1);
  interfaces1.SetForwarding (1, true);
  interfaces1.SetDefaultRouteInAllNodes (1);

  // Add second prefix to R interface connected to n0
  Ptr<Ipv6> ipv6Router = router.Get (0)->GetObject<Ipv6> ();
  uint32_t ifIndexR_n0 = ipv6Router->GetInterfaceForDevice (devices0.Get (1));
  ipv6Router->AddAddress (ifIndexR_n0, Ipv6InterfaceAddress (Ipv6Address ("2001:ABCD::1"), Ipv6Prefix (64)));

  // Enable RA on router for both subnets
  Ipv6InterfaceContainer allInterfaces;
  allInterfaces.Add (interfaces0);
  allInterfaces.Add (interfaces1);

  Ipv6Address network0_1 = Ipv6Address ("2001:1::");
  Ipv6Address network0_2 = Ipv6Address ("2001:ABCD::");
  Ipv6Address network1 = Ipv6Address ("2001:2::");

  for (uint32_t i = 0; i < allInterfaces.GetN (); ++i)
    {
      Ptr<Ipv6Interface> interface = allInterfaces.GetInterface (i);
      interface->SetAttribute ("BaseReachableTime", TimeValue (Seconds (30)));
      interface->SetAttribute ("RetransmissionTimer", TimeValue (Seconds (1)));
    }

  Ipv6InterfaceContainer routerInterfaces;
  routerInterfaces.Add (interfaces0.Get (1));
  routerInterfaces.Add (interfaces1.Get (1));

  Icmpv6RAHeader raHeader;
  raHeader.SetFlagM (false);
  raHeader.SetFlagO (false);
  raHeader.SetRouterLifetime (Seconds (30));
  raHeader.SetReachableTime (Seconds (0));
  raHeader.SetRetransmissionTime (Seconds (0));

  // Schedule periodic RA transmissions
  Simulator::Schedule (Seconds (2), &Ipv6Interface::SendRA, interfaces0.Get (1), raHeader, Ipv6Address::GetAllNodesMulticast (), 0);
  Simulator::Schedule (Seconds (2), &Ipv6Interface::SendRA, interfaces1.Get (1), raHeader, Ipv6Address::GetAllNodesMulticast (), 0);
  Simulator::Schedule (Seconds (2 + 3), &Ipv6Interface::SendRA, interfaces0.Get (1), raHeader, Ipv6Address::GetAllNodesMulticast (), 0);
  Simulator::Schedule (Seconds (2 + 3), &Ipv6Interface::SendRA, interfaces1.Get (1), raHeader, Ipv6Address::GetAllNodesMulticast (), 0);

  // Configure Prefix Information option for 2001:1::/64
  Icmpv6OptionPrefixInformation optPrefix1;
  optPrefix1.SetPrefix (network0_1, 64);
  optPrefix1.SetOnLinkFlag (true);
  optPrefix1.SetAutonomousFlag (true);
  optPrefix1.SetValidTime (Seconds (3600));
  optPrefix1.SetPreferredTime (Seconds (1800));
  Simulator::Schedule (Seconds (2), &Ipv6Interface::SendRAWithPrefix, interfaces0.Get (1), raHeader, optPrefix1, Ipv6Address::GetAllNodesMulticast ());

  // Configure Prefix Information option for 2001:ABCD::/64
  Icmpv6OptionPrefixInformation optPrefix2;
  optPrefix2.SetPrefix (Ipv6Address ("2001:ABCD::"), 64);
  optPrefix2.SetOnLinkFlag (true);
  optPrefix2.SetAutonomousFlag (true);
  optPrefix2.SetValidTime (Seconds (3600));
  optPrefix2.SetPreferredTime (Seconds (1800));
  Simulator::Schedule (Seconds (2), &Ipv6Interface::SendRAWithPrefix, interfaces0.Get (1), raHeader, optPrefix2, Ipv6Address::GetAllNodesMulticast ());

  // Configure Prefix Information option for 2001:2::/64
  Icmpv6OptionPrefixInformation optPrefix3;
  optPrefix3.SetPrefix (network1, 64);
  optPrefix3.SetOnLinkFlag (true);
  optPrefix3.SetAutonomousFlag (true);
  optPrefix3.SetValidTime (Seconds (3600));
  optPrefix3.SetPreferredTime (Seconds (1800));
  Simulator::Schedule (Seconds (2), &Ipv6Interface::SendRAWithPrefix, interfaces1.Get (1), raHeader, optPrefix3, Ipv6Address::GetAllNodesMulticast ());

  // Install Ping application from n0 to n1
  PingHelper ping6 (interfaces1.GetAddress (0, 1)); // n1's address on its interface
  ping6.SetAttribute ("Interval", TimeValue (MilliSeconds (pingInterval)));
  ping6.SetAttribute ("Size", UintegerValue (pingSize));
  ApplicationContainer apps = ping6.Install (nodes.Get (0));
  apps.Start (Seconds (4.0));
  apps.Stop (Seconds (simDuration));

  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("radvd-two-prefix.tr"));
  csma.EnablePcapAll ("radvd-two-prefix");

  NS_LOG_INFO ("Print IP addresses.");
  for (uint32_t i = 0; i < interfaces0.GetN (); ++i)
    {
      std::cout << "Node " << i << " IPv6 addresses:" << std::endl;
      for (uint32_t j = 0; j < interfaces0.Get (i)->GetNAddresses (); ++j)
        {
          std::cout << "  " << interfaces0.Get (i)->GetAddress (j).GetAddress () << std::endl;
        }
    }
  for (uint32_t i = 0; i < interfaces1.GetN (); ++i)
    {
      std::cout << "Node " << i << " IPv6 addresses:" << std::endl;
      for (uint32_t j = 0; j < interfaces1.Get (i)->GetNAddresses (); ++j)
        {
          std::cout << "  " << interfaces1.Get (i)->GetAddress (j).GetAddress () << std::endl;
        }
    }

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (simDuration + 1));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}