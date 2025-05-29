#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoChainExample");

void
TxTrace (Ptr<const Packet> packet)
{
  NS_LOG_UNCOND ("TX: " << Simulator::Now ().GetSeconds () << "s, Packet UID: " << packet->GetUid ());
}

void
RxTrace (Ptr<const Packet> packet, const Address &from)
{
  NS_LOG_UNCOND ("RX: " << Simulator::Now ().GetSeconds () << "s, Packet UID: " << packet->GetUid ());
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (6);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices[5];
  for (uint32_t i = 0; i < 5; ++i)
    {
      NodeContainer pair = NodeContainer (nodes.Get (i), nodes.Get (i + 1));
      devices[i] = pointToPoint.Install (pair);
    }

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces[5];
  for (uint32_t i = 0; i < 5; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i + 1 << ".0";
      address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
      interfaces[i] = address.Assign (devices[i]);
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // UDP Echo Server on node 5
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (5));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP Echo Client on node 0
  Ipv4Address serverAddress = interfaces[4].GetAddress (1);
  UdpEchoClientHelper echoClient (serverAddress, echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Tracing packet transmission and reception
  Ptr<Node> clientNode = nodes.Get (0);
  Ptr<NetDevice> clientDevice = devices[0].Get (0);
  clientDevice->GetObject<PointToPointNetDevice> ()->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&TxTrace));

  Ptr<Node> serverNode = nodes.Get (5);
  Ptr<NetDevice> serverDevice = devices[4].Get (1);
  serverDevice->GetObject<PointToPointNetDevice> ()->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxTrace));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}