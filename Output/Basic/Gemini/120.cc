#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/on-off-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = true;
  bool printRoutingTables = false;
  Time routingTablePrintInterval = Seconds (10);
  std::string animFile = "adhoc-aodv.xml";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("printRoutingTables", "Print routing tables periodically", printRoutingTables);
  cmd.AddValue ("routingTablePrintInterval", "Routing table printing interval", routingTablePrintInterval);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::AodvRoutingProtocol::EnableHello", BooleanValue (true));
  Config::SetDefault ("ns3::AodvRoutingProtocol::HelloInterval", TimeValue (Seconds (1)));

  NodeContainer nodes;
  nodes.Create (10);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (10.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (4),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 0, 50, 50)));
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  AodvHelper aodv;
  internet.SetRoutingHelper (aodv);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (nodes);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (20.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (4.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (20.0));

  for (uint32_t i = 1; i < nodes.GetN (); ++i)
  {
    UdpEchoClientHelper client (interfaces.GetAddress ((i + 1) % nodes.GetN ()), 9);
    client.SetAttribute ("MaxPackets", UintegerValue (5));
    client.SetAttribute ("Interval", TimeValue (Seconds (4.0)));
    client.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer apps = client.Install (nodes.Get (i));
    apps.Start (Seconds (2.0 + i * 0.1));
    apps.Stop (Seconds (20.0));
  }

  if (enablePcap)
  {
    for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      PcapHelper pcapHelper;
      pcapHelper.EnablePcap ("adhoc-aodv-" + std::to_string (i), interfaces.GetDevice (i), false, true);
    }
  }

  if (printRoutingTables)
  {
    Simulator::Schedule (routingTablePrintInterval, &Ipv4::PrintRoutingTableAllAt, routingTablePrintInterval, nodes);
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  AnimationInterface anim (animFile);

  Simulator::Stop (Seconds (20.0));

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
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Delay Sum:  " << i->second.delaySum << "\n";
    std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
    double packetLossRatio = (double)i->second.lostPackets / (double)i->second.txPackets;
        std::cout << "  Packet Loss Ratio: " << packetLossRatio << std::endl;

        double endToEndDelay = i->second.delaySum.GetSeconds() / i->second.rxPackets;
        std::cout << "  End-to-End Delay: " << endToEndDelay << " s" << std::endl;

        double throughput = (i->second.rxBytes * 8.0) / (Simulator::Now().GetSeconds() - 2.0);
        std::cout << "  Throughput: " << throughput / 1000000 << " Mbps" << std::endl;
  }

  monitor->SerializeToXmlFile("adhoc-aodv-flowmon.xml", true, true);

  Simulator::Destroy ();

  return 0;
}