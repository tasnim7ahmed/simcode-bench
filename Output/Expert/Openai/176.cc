#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/packet-sink.h"

using namespace ns3;

int 
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Configure simulation parameters
  double simTime = 5.0; // seconds
  uint32_t packetSize = 1024; // bytes
  uint32_t nPackets = 1000;
  double dataRateAppMbps = 8.0; // Sending rate in Mbps
  std::string linkDataRate = "1Mbps";
  std::string linkDelay = "2ms";
  uint32_t queueMaxPackets = 5; // Small queue to induce loss

  // Nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Point-to-Point link with limited queue size
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (linkDataRate));
  p2p.SetChannelAttribute ("Delay", StringValue (linkDelay));
  p2p.SetQueue ("ns3::DropTailQueue<Packet>", 
                "MaxPackets", UintegerValue (queueMaxPackets));

  NetDeviceContainer devices = p2p.Install (nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Install UDP server (sink) on node 1
  uint16_t port = 9000;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", 
                              InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime + 1.0));

  // Install UDP client (sender) on node 0
  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (nPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds (
    (packetSize * 8.0) / (dataRateAppMbps * 1e6) // seconds per packet
  )));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (simTime));

  // Enable NetAnim
  AnimationInterface anim ("packet-loss-netanim.xml");
  anim.SetConstantPosition (nodes.Get (0), 10.0, 20.0);
  anim.SetConstantPosition (nodes.Get (1), 40.0, 20.0);
  anim.EnablePacketMetadata (true);

  // Enable PCAP tracing
  p2p.EnablePcapAll ("packet-loss-example", true);

  // Schedule simulation end
  Simulator::Stop (Seconds (simTime + 1.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}