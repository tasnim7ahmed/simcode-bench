#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ospf-routing-helper.h"

using namespace ns3;

void
PrintRoutingTable (Ptr<Node> node, Time printTime)
{
  Ipv4StaticRoutingHelper staticRoutingHelper;
  Ipv4RoutingHelper* routingHelper = nullptr;
  Ipv4RoutingProtocol* protocol = node->GetObject<Ipv4>()->GetRoutingProtocol ();
  if (protocol)
    {
      std::ostringstream oss;
      protocol->PrintRoutingTable (
        oss,
        Time::ConvertPrecision (printTime, Time::S), true);
      std::cout << "Routing table of node " << node->GetId ()
                << " at time " << printTime.GetSeconds () << "s:"
                << std::endl << oss.str ();
    }
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (4);

  // Square topology: 0-1, 1-2, 2-3, 3-0
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer n0n1 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer n1n2 = p2p.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer n2n3 = p2p.Install (nodes.Get (2), nodes.Get (3));
  NetDeviceContainer n3n0 = p2p.Install (nodes.Get (3), nodes.Get (0));

  OspfRoutingHelper ospfRouting;
  InternetStackHelper stack;
  stack.SetRoutingHelper (ospfRouting);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces.push_back (address.Assign (n0n1));

  address.SetBase ("10.1.2.0", "255.255.255.0");
  interfaces.push_back (address.Assign (n1n2));

  address.SetBase ("10.1.3.0", "255.255.255.0");
  interfaces.push_back (address.Assign (n2n3));

  address.SetBase ("10.1.4.0", "255.255.255.0");
  interfaces.push_back (address.Assign (n3n0));

  // Enable pcap tracing on each link
  p2p.EnablePcap ("n0n1", n0n1, true);
  p2p.EnablePcap ("n1n2", n1n2, true);
  p2p.EnablePcap ("n2n3", n2n3, true);
  p2p.EnablePcap ("n3n0", n3n0, true);

  uint16_t serverPort = 4000;
  UdpServerHelper serverHelper (serverPort);
  ApplicationContainer serverApp = serverHelper.Install (nodes.Get (2));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  UdpClientHelper clientHelper (interfaces[1].GetAddress (1), serverPort);
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (100));
  clientHelper.SetAttribute ("Interval", TimeValue (MilliSeconds (100)));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApp = clientHelper.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));

  Simulator::Schedule (Seconds (10.5),
                       &PrintRoutingTable, nodes.Get (0), Seconds (10.5));
  Simulator::Schedule (Seconds (10.5),
                       &PrintRoutingTable, nodes.Get (1), Seconds (10.5));
  Simulator::Schedule (Seconds (10.5),
                       &PrintRoutingTable, nodes.Get (2), Seconds (10.5));
  Simulator::Schedule (Seconds (10.5),
                       &PrintRoutingTable, nodes.Get (3), Seconds (10.5));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}