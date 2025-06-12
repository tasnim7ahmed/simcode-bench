#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

static uint32_t txPackets[3] = {0, 0, 0};
static uint32_t rxPackets[3] = {0, 0, 0};

void
TxCallback (Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t interface)
{
  uint32_t nodeIndex = ipv4->GetObject<Node> ()->GetId ();
  txPackets[nodeIndex]++;
}

void
RxCallback (Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t interface)
{
  uint32_t nodeIndex = ipv4->GetObject<Node> ()->GetId ();
  rxPackets[nodeIndex]++;
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d0d1 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer d1d2 = p2p.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign (d0d1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign (d1d2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (i1i2.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (2));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  OnOffHelper client ("ns3::TcpSocketFactory", sinkAddress);
  client.SetAttribute ("DataRate", StringValue ("2Mbps"));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  client.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (10.0));

  // Tracing: Count packets transmitted and received
  for (uint32_t i = 0; i < 3; ++i)
    {
      Ptr<Ipv4> ipv4 = nodes.Get (i)->GetObject<Ipv4> ();
      ipv4->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxCallback, ipv4));
      ipv4->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&RxCallback, ipv4));
    }

  p2p.EnablePcapAll ("basic-wired-p2p");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  for (uint32_t i = 0; i < 3; ++i)
    {
      std::cout << "Node " << i << " transmitted packets: " << txPackets[i] << std::endl;
      std::cout << "Node " << i << " received packets: " << rxPackets[i] << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}