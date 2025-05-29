#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Set up point-to-point link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set up the TCP server (PacketSink) on Computer 2
  uint16_t port = 9;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (20.0));

  // Set up the TCP client (OnOffApplication) on Computer 1
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", sinkAddress);
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (1024 * 10));
  clientHelper.SetAttribute ("DataRate", StringValue ("8192bps")); // 1024 bytes per sec (8 * 1024 = 8192bps)
  clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer clientApp = clientHelper.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (20.0));

  // Enable Pcap tracing
  pointToPoint.EnablePcapAll ("tcp-wired-2node");

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}