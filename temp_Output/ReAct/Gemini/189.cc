#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaTcpExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("CsmaTcpExample", LOG_LEVEL_INFO);
    }

  uint32_t nCsma = 4;

  NodeContainer csmaNodes;
  csmaNodes.Create (nCsma);

  NodeContainer serverNode;
  serverNode.Create (1);

  NodeContainer allNodes;
  allNodes.Add (serverNode);
  allNodes.Add (csmaNodes);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer csmaDevices = csma.Install (csmaNodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer serverDevice = pointToPoint.Install (serverNode, csmaNodes.Get(0));

  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces = address.Assign (csmaDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer serverInterface = address.Assign (serverDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (serverNode.Get (0));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  for (uint32_t i = 0; i < nCsma; ++i)
    {
      BulkSendHelper source ("ns3::TcpSocketFactory",
                             InetSocketAddress (serverInterface.GetAddress (0), port));
      source.SetAttribute ("SendSize", UintegerValue (1400));
      ApplicationContainer sourceApps = source.Install (csmaNodes.Get (i));
      sourceApps.Start (Seconds (2.0 + i * 0.1));
      sourceApps.Stop (Seconds (9.0 + i * 0.1));
    }

  if (tracing)
    {
      csma.EnablePcap ("csma-tcp", csmaDevices, true);
      pointToPoint.EnablePcapAll ("csma-tcp");
    }

  AnimationInterface anim ("csma-tcp-animation.xml");
  anim.SetConstantPosition (serverNode.Get (0), 10, 10);
  for (uint32_t i = 0; i < nCsma; ++i)
    {
      anim.SetConstantPosition (csmaNodes.Get (i), 20 + i * 10, 20);
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (auto i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
      std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
    }

  monitor->SerializeToXmlFile("csma-tcp-flowmon.xml", true, true);

  Simulator::Destroy ();
  return 0;
}