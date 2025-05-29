#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingTopologyExample");

int
main (int argc, char *argv[])
{
  LogComponentEnable ("RingTopologyExample", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Create 3 nodes
  NodeContainer nodes;
  nodes.Create (3);

  // Create point-to-point links (ring: 0-1, 1-2, 2-0)
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Link 0-1
  NetDeviceContainer d01 = p2p.Install (nodes.Get (0), nodes.Get (1));
  // Link 1-2
  NetDeviceContainer d12 = p2p.Install (nodes.Get (1), nodes.Get (2));
  // Link 2-0
  NetDeviceContainer d20 = p2p.Install (nodes.Get (2), nodes.Get (0));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces;

  // 0-1
  address.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces.push_back (address.Assign (d01));
  // 1-2
  address.SetBase ("10.1.2.0", "255.255.255.0");
  interfaces.push_back (address.Assign (d12));
  // 2-0
  address.SetBase ("10.1.3.0", "255.255.255.0");
  interfaces.push_back (address.Assign (d20));

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install UDP Server on Node 2 (will listen on port 4000)
  uint16_t serverPort = 4000;
  UdpServerHelper server (serverPort);
  ApplicationContainer serverApp = server.Install (nodes.Get (2));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (10.0));

  // Install UDP Client on Node 0, destination: Node 2
  UdpClientHelper client (interfaces[1].GetAddress (1), serverPort); // 1st index is device 1 on 1-2 (Node2)
  client.SetAttribute ("MaxPackets", UintegerValue (5));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (10.0));

  // Tracing packet transmission on the NetDevices
  for (uint32_t i = 0; i < 3; ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      for (uint32_t j = 0; j < node->GetNDevices (); ++j)
        {
          Ptr<NetDevice> dev = node->GetDevice (j);
          dev->TraceConnectWithoutContext ("PhyTxBegin", MakeCallback ([] (Ptr<const Packet> p) {
            NS_LOG_INFO ("Packet transmitted at " << Simulator::Now ().GetSeconds () << "s, Size: " << p->GetSize ());
          }));
        }
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}