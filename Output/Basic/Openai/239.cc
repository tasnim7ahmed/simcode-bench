#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpClientServerP2PRouter");

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Create 3 nodes: client, router, server
  NodeContainer nodes;
  nodes.Create (3);
  Ptr<Node> client = nodes.Get (0);
  Ptr<Node> router = nodes.Get (1);
  Ptr<Node> server = nodes.Get (2);

  // Create Point-to-Point links
  NodeContainer n0n1 = NodeContainer (client, router);
  NodeContainer n1n2 = NodeContainer (router, server);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer dev0 = p2p.Install (n0n1);
  NetDeviceContainer dev1 = p2p.Install (n1n2);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if0 = address.Assign (dev0);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if1 = address.Assign (dev1);

  // Enable routing on router node
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install server application on server node
  uint16_t port = 50000;
  Address serverAddress (InetSocketAddress (if1.GetAddress (1), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sinkHelper.Install (server);
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  // Install OnOff application on client node
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", serverAddress);
  clientHelper.SetAttribute ("DataRate", StringValue ("1Mbps"));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  clientHelper.SetAttribute ("StopTime", TimeValue (Seconds (9.0)));

  ApplicationContainer clientApp = clientHelper.Install (client);

  // Enable ASCII tracing
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("tcp-p2p-router.tr"));

  // Enable pcap tracing
  p2p.EnablePcapAll ("tcp-p2p-router");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}