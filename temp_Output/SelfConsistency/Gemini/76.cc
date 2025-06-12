#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/packet-sink.h"
#include "ns3/queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedNetwork");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NS_LOG_INFO ("Create nodes.");
  NodeContainer p2pNodes;
  p2pNodes.Create (3);

  NodeContainer csmaNodes;
  csmaNodes.Add (p2pNodes.Get (2));
  csmaNodes.Create (3);

  NodeContainer p2pNodes2;
  p2pNodes2.Add (csmaNodes.Get (3));
  p2pNodes2.Create (1);

  NodeContainer allNodes;
  allNodes.Add (p2pNodes);
  allNodes.Add (csmaNodes.Get (1));
  allNodes.Add (csmaNodes.Get (2));
  allNodes.Add (p2pNodes2.Get (1));

  NS_LOG_INFO ("Create channels.");
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes.Get (0), p2pNodes.Get (2));

  NetDeviceContainer p2pDevices2;
  p2pDevices2 = pointToPoint.Install (p2pNodes.Get (1), p2pNodes.Get (2));

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  NetDeviceContainer p2pDevices3;
  p2pDevices3 = pointToPoint.Install (csmaNodes.Get (3), p2pNodes2.Get (1));

  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces2;
  p2pInterfaces2 = address.Assign (p2pDevices2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces3;
  p2pInterfaces3 = address.Assign (p2pDevices3);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Set up UDP CBR flow from n0 to n6

  uint16_t port = 9;  // Discard port (RFC 863)

  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (p2pInterfaces3.GetAddress (1), port)));
  onoff.SetConstantRate (DataRate ("300bps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (50));

  ApplicationContainer apps = onoff.Install (p2pNodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  // Create a packet sink to receive these packets
  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         InetSocketAddress (p2pInterfaces3.GetAddress (1), port));
  apps = sink.Install (p2pNodes2.Get (1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  // Add tracing
  if (tracing)
    {
      pointToPoint.EnablePcapAll ("mixed-network");
      csma.EnablePcapAll ("mixed-network", false);
    }

  // Trace queue occupancy on the CSMA channel.
  for (uint32_t i = 0; i < csmaDevices.GetN (); ++i)
    {
      Ptr<Queue> queue = StaticCast<CsmaNetDevice> (csmaDevices.Get (i))->GetQueue ();
      if (queue)
        {
          Simulator::ScheduleWithContext (queue->GetNode ()->GetId (),
                                          Seconds (0.1),
                                          MakeEvent (&Queue::TraceQueueSize, queue));
        }
    }

   // Trace packet receptions at node 6
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("mixed-network-packet-reception.txt");
  Config::ConnectWithoutContext("/NodeList/6/ApplicationList/*/$ns3::PacketSink/Rx", MakeBoundCallback(&AsciiTraceHelper::TraceRx, &ascii, stream));

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}