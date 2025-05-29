#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("GlobalRoutingSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (7);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (100)));

  NetDeviceContainer p2pDevices1, p2pDevices2, p2pDevices3;
  NetDeviceContainer csmaDevices1, csmaDevices2;

  p2pDevices1 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  p2pDevices2 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));
  p2pDevices3 = pointToPoint.Install (nodes.Get (3), nodes.Get (6));
  csmaDevices1 = csma.Install (NodeContainer (nodes.Get (2), nodes.Get (3), nodes.Get (4)));
  csmaDevices2 = csma.Install (NodeContainer (nodes.Get (4), nodes.Get (5), nodes.Get (6)));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces1 = address.Assign (p2pDevices1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces2 = address.Assign (p2pDevices2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces1 = address.Assign (csmaDevices1);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces3 = address.Assign (p2pDevices3);

  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces2 = address.Assign (csmaDevices2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  Ipv4GlobalRoutingHelper::PopulateRoutingTables (true);

  uint16_t port = 9;

  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     AddressValue (InetSocketAddress (p2pInterfaces3.GetAddress (1), port)));
  onoff.SetConstantRate (DataRate ("1Mbps"));

  ApplicationContainer app1 = onoff.Install (nodes.Get (0));
  app1.Start (Seconds (1.0));
  app1.Stop (Seconds (20.0));

  OnOffHelper onoff2 ("ns3::UdpSocketFactory",
                     AddressValue (InetSocketAddress (p2pInterfaces3.GetAddress (1), port)));
  onoff2.SetConstantRate (DataRate ("1Mbps"));

  ApplicationContainer app2 = onoff2.Install (nodes.Get (0));
  app2.Start (Seconds (11.0));
  app2.Stop (Seconds (20.0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (nodes.Get (6));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (25.0));

  Simulator::Schedule (Seconds (5.0), [&]() {
      Ptr<NetDevice> dev = p2pDevices2.Get (0);
      dev->SetAttribute ("Mtu", UintegerValue (1400));
      dev->SetAttribute ("TxQueue/MaxPackets", UintegerValue (1));
      dev->SetAttribute ("TxQueue/Mode", EnumValue (Queue::QUEUE_MODE_BYTES));
      dev->SetAttribute ("TxQueue/MaxBytes", UintegerValue (1000));
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (0));
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (6));
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (1));
    });

  Simulator::Schedule (Seconds (7.0), [&]() {
      Ptr<NetDevice> dev = p2pDevices2.Get (0);
      dev->SetAttribute ("Mtu", UintegerValue (1500));
      dev->SetAttribute ("TxQueue/MaxPackets", UintegerValue (1000));
      dev->SetAttribute ("TxQueue/Mode", EnumValue (Queue::QUEUE_MODE_PACKETS));
      dev->SetAttribute ("TxQueue/MaxBytes", UintegerValue (0));
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (0));
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (6));
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (1));
    });

  Simulator::Schedule (Seconds (13.0), [&]() {
      Ptr<Node> n1 = nodes.Get (1);
      Ptr<NetDevice> dev = n1->GetDevice (0);
      dev->SetIfDown ();
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (0));
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (6));
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (1));
    });

   Simulator::Schedule (Seconds (16.0), [&]() {
      Ptr<Node> n1 = nodes.Get (1);
      Ptr<NetDevice> dev = n1->GetDevice (0);
      dev->SetIfUp ();
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (0));
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (6));
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (1));
    });

  Simulator::Schedule (Seconds (17.0), [&]() {
      Ptr<Node> n2 = nodes.Get (6);
      Ptr<NetDevice> dev = n2->GetDevice (0);
      dev->SetIfDown ();
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (0));
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (6));
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (1));
    });

   Simulator::Schedule (Seconds (18.0), [&]() {
      Ptr<Node> n2 = nodes.Get (6);
      Ptr<NetDevice> dev = n2->GetDevice (0);
      dev->SetIfUp ();
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (0));
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (6));
      Ipv4GlobalRoutingHelper::RecomputeRoutingTable (nodes.Get (1));
    });

  pointToPoint.EnablePcapAll ("global-routing");
  csma.EnablePcapAll ("global-routing");

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("global-routing.tr"));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}