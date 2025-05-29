#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingTopologyExample");

class PacketLogger
{
public:
  static void
  TxLogger (std::string context, Ptr<const Packet> packet)
  {
    NS_LOG_INFO ("TX: " << Simulator::Now ().GetSeconds () << "s " << context << " Packet UID: " << packet->GetUid ());
  }
  static void
  RxLogger (std::string context, Ptr<const Packet> packet, const Address &address)
  {
    NS_LOG_INFO ("RX: " << Simulator::Now ().GetSeconds () << "s " << context << " Packet UID: " << packet->GetUid ());
  }
};

int
main (int argc, char *argv[])
{
  LogComponentEnable ("RingTopologyExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer nd0 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer nd1 = p2p.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer nd2 = p2p.Install (nodes.Get (2), nodes.Get (0));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iface0 = address.Assign (nd0);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iface1 = address.Assign (nd1);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iface2 = address.Assign (nd2);

  // UDP Echo Applications to generate traffic around the ring

  uint16_t port = 9;
  // Server on node 1 (receives from node 0)
  UdpEchoServerHelper echoServer0 (port);
  ApplicationContainer serverApps0 = echoServer0.Install (nodes.Get (1));
  serverApps0.Start (Seconds (1.0));
  serverApps0.Stop (Seconds (10.0));

  // Client on node 0 -> node 1
  UdpEchoClientHelper echoClient0 (iface0.GetAddress (1), port);
  echoClient0.SetAttribute ("MaxPackets", UintegerValue (3));
  echoClient0.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient0.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps0 = echoClient0.Install (nodes.Get (0));
  clientApps0.Start (Seconds (2.0));
  clientApps0.Stop (Seconds (10.0));

  // Server on node 2 (receives from node 1)
  UdpEchoServerHelper echoServer1 (port+1);
  ApplicationContainer serverApps1 = echoServer1.Install (nodes.Get (2));
  serverApps1.Start (Seconds (1.0));
  serverApps1.Stop (Seconds (10.0));
  // Client on node 1 -> node 2
  UdpEchoClientHelper echoClient1 (iface1.GetAddress (1), port+1);
  echoClient1.SetAttribute ("MaxPackets", UintegerValue (3));
  echoClient1.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps1 = echoClient1.Install (nodes.Get (1));
  clientApps1.Start (Seconds (3.0));
  clientApps1.Stop (Seconds (10.0));

  // Server on node 0 (receives from node 2)
  UdpEchoServerHelper echoServer2 (port+2);
  ApplicationContainer serverApps2 = echoServer2.Install (nodes.Get (0));
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (10.0));
  // Client on node 2 -> node 0
  UdpEchoClientHelper echoClient2 (iface2.GetAddress (1), port+2);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (3));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps2 = echoClient2.Install (nodes.Get (2));
  clientApps2.Start (Seconds (4.0));
  clientApps2.Stop (Seconds (10.0));

  // Enable packet logging
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/PhyTxEnd",
                   MakeCallback (&PacketLogger::TxLogger));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacRx",
                   MakeCallback (&PacketLogger::RxLogger));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}