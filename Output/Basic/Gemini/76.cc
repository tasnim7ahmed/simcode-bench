#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedNetwork");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LOG_LEVEL_INFO);
  LogComponent::SetPrintLevel (LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (7);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices02 = pointToPoint.Install (nodes.Get (0), nodes.Get (2));
  NetDeviceContainer p2pDevices12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer p2pDevices56 = pointToPoint.Install (nodes.Get (5), nodes.Get (6));

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("3ms"));

  NetDeviceContainer csmaDevices = csma.Install (NodeContainer (nodes.Get (2), nodes.Get (3), nodes.Get (4), nodes.Get (5)));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces02 = address.Assign (p2pDevices02);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces12 = address.Assign (p2pDevices12);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces = address.Assign (csmaDevices);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces56 = address.Assign (p2pDevices56);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (p2pInterfaces56.GetAddress (1), port)));
  onoff.SetConstantRate (DataRate ("300bps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (50));

  ApplicationContainer app = onoff.Install (nodes.Get (0));
  app.Start (Seconds (1.0));
  app.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (nodes.Get (6));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  // Enable Tracing
  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("mixed-network.tr"));
  pointToPoint.EnablePcapAll ("mixed-network");
  csma.EnableAsciiAll (ascii.CreateFileStream ("mixed-network-csma.tr"));
  csma.EnablePcapAll ("mixed-network-csma");

  // Queue Tracing
  for (int i = 0; i < 7; i++)
    {
      if (i != 6){
        std::string sink_queue_name = "/NodeList/" + std::to_string(i) + "/$ns3::TrafficControlLayer/RootQueue";
        Simulator::Trace("/NodeList/" + std::to_string(i) + "/$ns3::TrafficControlLayer/RootQueue/Drop", sink_queue_name + "/Drop");
      }

    }

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();


  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000  << " kbps\n";
    }

  monitor->SerializeToXmlFile("mixed-network.flowmon", true, true);
  Simulator::Destroy ();

  return 0;
}