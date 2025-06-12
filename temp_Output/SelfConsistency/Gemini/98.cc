#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/queue.h"
#include "ns3/queue-disc.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("GlobalRoutingMixedNetwork");

// Callback function to trace queue occupancy
void QueueSizeChange (std::string context, uint32_t queueSize)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s " << context << " Queue Size: " << queueSize);
}


int
main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true.", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (7);

  NodeContainer p2pNodes1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer p2pNodes2 = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer p2pNodes3 = NodeContainer (nodes.Get (2), nodes.Get (6));
  NodeContainer p2pNodes4 = NodeContainer (nodes.Get (0), nodes.Get (3));
  NodeContainer p2pNodes5 = NodeContainer (nodes.Get (3), nodes.Get (4));
  NodeContainer p2pNodes6 = NodeContainer (nodes.Get (4), nodes.Get (6));

  NodeContainer csmaNodes = NodeContainer (nodes.Get (3), nodes.Get (5), nodes.Get (4));

  NS_LOG_INFO ("Create channels.");
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices1 = pointToPoint.Install (p2pNodes1);
  NetDeviceContainer p2pDevices2 = pointToPoint.Install (p2pNodes2);
  NetDeviceContainer p2pDevices3 = pointToPoint.Install (p2pNodes3);
  NetDeviceContainer p2pDevices4 = pointToPoint.Install (p2pNodes4);
  NetDeviceContainer p2pDevices5 = pointToPoint.Install (p2pNodes5);
  NetDeviceContainer p2pDevices6 = pointToPoint.Install (p2pNodes6);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("3ms"));
  NetDeviceContainer csmaDevices = csma.Install (csmaNodes);

  NS_LOG_INFO ("Assign IP Addresses.");
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces1 = address.Assign (p2pDevices1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces2 = address.Assign (p2pDevices2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces3 = address.Assign (p2pDevices3);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces4 = address.Assign (p2pDevices4);

  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces5 = address.Assign (p2pDevices5);

  address.SetBase ("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces6 = address.Assign (p2pDevices6);

  address.SetBase ("10.1.7.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces = address.Assign (csmaDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  Ipv4GlobalRoutingHelper::PopulateRoutingTables (); // call it twice! otherwise it won't work

  // Enable automatic response to interface events
  Ipv4GlobalRoutingHelper::RespondToInterfaceEvents ();

  NS_LOG_INFO ("Create Applications.");

  uint16_t port = 9; // Discard port (RFC 793)
  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (6));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (20.0));

  UdpClientHelper client (Ipv4Address ("10.1.3.2"), port); // address of node 6
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps1 = client.Install (nodes.Get (0));
  clientApps1.Start (Seconds (1.0));
  clientApps1.Stop (Seconds (10.0));

  ApplicationContainer clientApps2 = client.Install (nodes.Get (0));
  clientApps2.Start (Seconds (11.0));
  clientApps2.Stop (Seconds (20.0));

  // Trigger interface down/up events

  Simulator::Schedule (Seconds (5.0), &Ipv4Interface::SetDown, p2pInterfaces1.Get (0));
  Simulator::Schedule (Seconds (7.0), &Ipv4Interface::SetUp, p2pInterfaces1.Get (0));
  Simulator::Schedule (Seconds (13.0), &Ipv4Interface::SetDown, p2pInterfaces4.Get (0));
  Simulator::Schedule (Seconds (15.0), &Ipv4Interface::SetUp, p2pInterfaces4.Get (0));

  // Trace queue occupancy
  if (tracing) {
      // Example: Trace the queue on the first p2p link
      Ptr<Queue> queue = StaticCast<PointToPointNetDevice> (p2pDevices1.Get (0))->GetQueue ();
      if (queue != nullptr) {
          Config::ConnectWithoutContext ("/NodeList/" + std::to_string (0) + "/DeviceList/" + std::to_string (0) + "/$ns3::PointToPointNetDevice/TxQueue/Size", MakeCallback (&QueueSizeChange));
      } else {
          std::cerr << "Error: Queue not found on the specified device." << std::endl;
      }
  }

  NS_LOG_INFO ("Enable Tracing.");
  if (tracing)
    {
      pointToPoint.EnablePcapAll ("global-routing-mixed");
    }

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum:    " << i->second.delaySum << "\n";
      std::cout << "  Jitter Sum:   " << i->second.jitterSum << "\n";
    }

  monitor->SerializeToXmlFile("global-routing-mixed.flowmon", true, true);

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}