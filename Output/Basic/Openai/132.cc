#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DvTwoNodeLoopExample");

void
PrintRoutingTable (Ptr<Node> node, Time printTime, std::string nodeName)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  Ipv4RoutingTableEntry entry;
  std::ostringstream oss;
  Ipv4StaticRoutingHelper staticHelper;
  Ptr<Ipv4StaticRouting> staticRouting = staticHelper.GetStaticRouting (ipv4);
  oss << "*** " << nodeName << " Routing Table at " << printTime.GetSeconds () << "s ***\n";
  for (uint32_t i = 0; i < staticRouting->GetNRoutes (); ++i)
    {
      entry = staticRouting->GetRoute (i);
      oss << entry.GetDestNetwork () << "/" << entry.GetDestNetworkMask ().GetPrefixLength ()
          << " via " << entry.GetGateway ()
          << " dev " << entry.GetInterface () << "\n";
    }
  oss << "**************************\n";
  std::cout << oss.str ();
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("DvTwoNodeLoopExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3); // Node 0: A, Node 1: B, Node 2: DST

  // Create point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devicesAB = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devicesAD = p2p.Install (nodes.Get (0), nodes.Get (2));
  NetDeviceContainer devicesBD = p2p.Install (nodes.Get (1), nodes.Get (2));

  // Install Internet stack
  InternetStackHelper internet;
  Ipv4ListRoutingHelper list;
  Ipv4StaticRoutingHelper staticRouting;
  list.Add (staticRouting, 0);
  internet.SetRoutingHelper (list);
  internet.Install (nodes);

  // Assign IPs
  Ipv4AddressHelper addrAB, addrAD, addrBD;
  addrAB.SetBase ("10.0.1.0", "255.255.255.0");
  addrAD.SetBase ("10.0.2.0", "255.255.255.0");
  addrBD.SetBase ("10.0.3.0", "255.255.255.0");

  Ipv4InterfaceContainer ifAB = addrAB.Assign (devicesAB);
  Ipv4InterfaceContainer ifAD = addrAD.Assign (devicesAD);
  Ipv4InterfaceContainer ifBD = addrBD.Assign (devicesBD);

  // Short reference for nodes
  Ptr<Node> nodeA = nodes.Get (0);
  Ptr<Node> nodeB = nodes.Get (1);
  Ptr<Node> nodeD = nodes.Get (2);

  // Setup manual distance vector initial tables:
  // All nodes initially can reach D. A and B have direct and indirect routes to D.

  Ptr<Ipv4StaticRouting> staticA = staticRouting.GetStaticRouting (nodeA->GetObject<Ipv4> ());
  Ptr<Ipv4StaticRouting> staticB = staticRouting.GetStaticRouting (nodeB->GetObject<Ipv4> ());
  Ptr<Ipv4StaticRouting> staticD = staticRouting.GetStaticRouting (nodeD->GetObject<Ipv4> ());

  // Direct & indirect routes for 10.0.2.0/24 and 10.0.3.0/24 are local nets, so only need to setup 10.0.2.2 (/24) as destination

  // Node D is the destination, listens on 10.0.2.2 and 10.0.3.2

  // Node A: to D's 10.0.2.2 via ifAD (10.0.2.2 direct)
  staticA->AddHostRouteTo (Ipv4Address ("10.0.2.2"), Ipv4Address ("10.0.2.2"), 2); // via interface 2 (A's ifAD)

  // Node B: to D's 10.0.3.2 via ifBD (10.0.3.2 direct)
  staticB->AddHostRouteTo (Ipv4Address ("10.0.3.2"), Ipv4Address ("10.0.3.2"), 2); // via interface 2 (B's ifBD)

  // Both nodes also announce a route to 10.0.2.2 via each other (indirect path)
  staticA->AddHostRouteTo (Ipv4Address ("10.0.2.2"), Ipv4Address ("10.0.1.2"), 1); // via B over AB
  staticB->AddHostRouteTo (Ipv4Address ("10.0.2.2"), Ipv4Address ("10.0.1.1"), 1); // via A over AB

  // Node D doesn't need routes, it's a sink.

  // Print initial routing tables
  Simulator::Schedule (Seconds (0.5), &PrintRoutingTable, nodeA, Seconds (0.5), "NodeA");
  Simulator::Schedule (Seconds (0.5), &PrintRoutingTable, nodeB, Seconds (0.5), "NodeB");

  // Configure UDP echo server on node D (both interfaces)
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodeD);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Echo client on nodeA, will send to D's 10.0.2.2 ip
  UdpEchoClientHelper echoClient (Ipv4Address ("10.0.2.2"), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (30));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps = echoClient.Install (nodeA);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // At t=4s, simulate link failure between node D and both A and B
  // We do this by bringing down D's NetDevices
  Simulator::Schedule (Seconds (4.0), [&](){
      NS_LOG_INFO ("*** Simulating D failure (interfaces down) ***");
      nodeD->GetDevice (0)->SetReceiveCallback (MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &> ());
      nodeD->GetDevice (1)->SetReceiveCallback (MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &> ());
      nodeD->GetDevice (0)->SetMtu (68); // just to trigger something, but not necessary
      nodeD->GetObject<Ipv4> ()->SetDown (0); // bring down ifAD
      nodeD->GetObject<Ipv4> ()->SetDown (1); // bring down ifBD
    });

  // At t=4.1s, remove direct routes to D from A and B, only indirect routes remain (representing stale information)
  Simulator::Schedule (Seconds (4.1), [&](){
      NS_LOG_INFO ("*** Removing direct routes to D ***");
      // Remove direct route from A
      for (uint32_t i = staticA->GetNRoutes (); i > 0; --i)
        {
          Ipv4RoutingTableEntry ent = staticA->GetRoute (i-1);
          if (ent.GetDest () == Ipv4Address ("10.0.2.2") && ent.GetGateway () == Ipv4Address ("10.0.2.2"))
            staticA->RemoveRoute (i-1);
        }
      // Remove direct route from B
      for (uint32_t i = staticB->GetNRoutes (); i > 0; --i)
        {
          Ipv4RoutingTableEntry ent = staticB->GetRoute (i-1);
          if (ent.GetDest () == Ipv4Address ("10.0.3.2") && ent.GetGateway () == Ipv4Address ("10.0.3.2"))
            staticB->RemoveRoute (i-1);
        }
    });

  // At t=4.2s, print routing tables
  Simulator::Schedule (Seconds (4.2), &PrintRoutingTable, nodeA, Seconds (4.2), "NodeA");
  Simulator::Schedule (Seconds (4.2), &PrintRoutingTable, nodeB, Seconds (4.2), "NodeB");

  // Install a custom packet sniffing application on B to observe looping packets
  class SniffApp : public Application
  {
  public:
    SniffApp () {}
    void Setup (Ptr<NetDevice> dev) { m_dev = dev; }
  private:
    virtual void StartApplication ()
    {
      m_dev->SetReceiveCallback (MakeCallback (&SniffApp::ReceivePacket, this));
    }
    virtual void StopApplication ()
    {
      m_dev->SetReceiveCallback (MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &> ());
    }
    bool ReceivePacket (Ptr<NetDevice>, Ptr<const Packet> pkt, uint16_t, const Address &)
    {
      NS_LOG_INFO ("SniffApp saw packet of " << pkt->GetSize () << " bytes at " << Simulator::Now ().GetSeconds () << "s");
      return false;
    }
    Ptr<NetDevice> m_dev;
  };

  Ptr<SniffApp> sniffA = CreateObject<SniffApp> ();
  sniffA->Setup (nodeA->GetDevice (1)); // monitor AB side
  nodeA->AddApplication (sniffA);
  sniffA->SetStartTime (Seconds (3.5));
  sniffA->SetStopTime (Seconds (10.0));

  Ptr<SniffApp> sniffB = CreateObject<SniffApp> ();
  sniffB->Setup (nodeB->GetDevice (1)); // monitor AB side
  nodeB->AddApplication (sniffB);
  sniffB->SetStartTime (Seconds (3.5));
  sniffB->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}