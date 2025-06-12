#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ospf-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfLsaSimulation");

int 
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  LogComponentEnable ("OspfLsaSimulation", LOG_LEVEL_INFO);

  // Create 4 router nodes
  NodeContainer routers;
  routers.Create (4);

  // Point-to-point attributes
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Connect routers in a mesh topology
  NetDeviceContainer d01 = p2p.Install (routers.Get(0), routers.Get(1));
  NetDeviceContainer d12 = p2p.Install (routers.Get(1), routers.Get(2));
  NetDeviceContainer d23 = p2p.Install (routers.Get(2), routers.Get(3));
  NetDeviceContainer d30 = p2p.Install (routers.Get(3), routers.Get(0));
  NetDeviceContainer d02 = p2p.Install (routers.Get(0), routers.Get(2));
  NetDeviceContainer d13 = p2p.Install (routers.Get(1), routers.Get(3));

  // Install internet stack with OSPF
  OspfRoutingHelper ospfRouting;
  InternetStackHelper stack;
  stack.SetRoutingHelper (ospfRouting);
  stack.Install (routers);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if01 = ipv4.Assign (d01);

  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if12 = ipv4.Assign (d12);

  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if23 = ipv4.Assign (d23);

  ipv4.SetBase ("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer if30 = ipv4.Assign (d30);

  ipv4.SetBase ("10.0.5.0", "255.255.255.0");
  Ipv4InterfaceContainer if02 = ipv4.Assign (d02);

  ipv4.SetBase ("10.0.6.0", "255.255.255.0");
  Ipv4InterfaceContainer if13 = ipv4.Assign (d13);

  // Enable pcap tracing for all point-to-point links
  p2p.EnablePcapAll ("ospf-lsa");

  // Create UDP application to generate traffic from Router 0 to Router 2
  uint16_t port = 9999;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (routers.Get(2));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (20.0));

  UdpClientHelper client (if02.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (100));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  client.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer clientApp = client.Install (routers.Get(0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (20.0));

  // Simulate a link failure event to trigger LSAs
  Simulator::Schedule (Seconds (8.0), [&]{
    NS_LOG_INFO ("Simulating link failure between Router 1 and 2 at " << Simulator::Now().GetSeconds() << "s");
    d12.Get(0)->SetReceiveCallback (MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &>());
    d12.Get(1)->SetReceiveCallback (MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &>());
    d12.Get(0)->SetLinkUpCallback (MakeNullCallback<void>);
    d12.Get(0)->SetLinkDownCallback (MakeNullCallback<void>);
    d12.Get(1)->SetLinkUpCallback (MakeNullCallback<void>);
    d12.Get(1)->SetLinkDownCallback (MakeNullCallback<void>);
    d12.Get(0)->SetMtu (0);
    d12.Get(1)->SetMtu (0);
    d12.Get(0)->SetIfIndex (-1);
    d12.Get(1)->SetIfIndex (-1);
  });

  Simulator::Stop (Seconds (22.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}