/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingTcpPersistentExample");

int
main (int argc, char *argv[])
{
  LogComponentEnable ("RingTcpPersistentExample", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
  LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);

  // Create 4 nodes
  NodeContainer nodes;
  nodes.Create (4);

  // Create the ring by connecting adjacent nodes with point-to-point links
  // and finally connecting the last node to the first
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue("1ms"));

  // Each link is between nodes[i] and nodes[(i+1)%4]
  NetDeviceContainer devices[4];
  Ipv4InterfaceContainer interfaces[4];

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  char subnet[20];

  for (uint32_t i = 0; i < 4; ++i)
    {
      NodeContainer pair = NodeContainer (nodes.Get(i), nodes.Get((i+1)%4));
      devices[i] = p2p.Install (pair);

      // Assign IP addresses for each link subnet: 10.1.i.0
      std::snprintf (subnet, sizeof(subnet), "10.1.%u.0", i+1);
      address.SetBase (subnet, "255.255.255.0");
      interfaces[i] = address.Assign (devices[i]);
    }

  // Assign static positions for NetAnim
  AnimationInterface anim ("ring-topology.xml");
  anim.SetConstantPosition (nodes.Get(0), 0.0, 50.0);
  anim.SetConstantPosition (nodes.Get(1), 50.0, 100.0);
  anim.SetConstantPosition (nodes.Get(2), 100.0, 50.0);
  anim.SetConstantPosition (nodes.Get(3), 50.0, 0.0);

  // Enable pcap tracing on all p2p devices
  for (uint32_t i = 0; i < 4; ++i)
    {
      p2p.EnablePcapAll (std::string("ringtopo-") + std::to_string(i), false);
    }

  // Install routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // TCP Server on Node 2, listening on port 50000
  uint16_t port = 50000;
  Address serverAddress (InetSocketAddress (interfaces[1].GetAddress(1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer serverApp = packetSinkHelper.Install (nodes.Get(2));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (20.0));

  // Persistent TCP Client on Node 0, connecting to Node 2
  // Use OnOffApplication to maintain a long persistent flow
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", serverAddress);
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate("1Mbps")));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=20.0]"));
  clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));

  ApplicationContainer clientApp = clientHelper.Install (nodes.Get(0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (20.0));

  // Animate packet transmissions for NetAnim
  for (uint32_t i = 0; i < 4; ++i)
    {
      for (uint32_t j = 0; j < devices[i].GetN (); ++j)
        {
          anim.TraceConnectWithoutContext ("PacketTransmit", MakeCallback(&AnimationInterface::DefaultPacketTrace));
        }
    }

  NS_LOG_INFO ("Starting simulation...");
  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Simulation finished.");

  return 0;
}