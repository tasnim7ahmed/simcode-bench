#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetAodvSimulation");

int main (int argc, char *argv[])
{
  bool tracing = false;
  uint32_t packetSize = 1024;
  std::string dataRate = "5Mbps";
  std::string delay = "2ms";
  uint32_t numNodes = 4;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable or disable tracing", tracing);
  cmd.AddValue ("packetSize", "Size of UDP packets", packetSize);
  cmd.AddValue ("dataRate", "Data rate of the link", dataRate);
  cmd.AddValue ("delay", "Delay of the link", delay);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (numNodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingProtocol ("ns3::AodvHelper", "EnableHello", BooleanValue (true));
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 0, 100, 100)));
  mobility.Install (nodes);

  UdpClientServerHelper udpClientServer (interfaces.GetAddress (numNodes - 1), 9);
  udpClientServer.SetAttribute ("PacketSize", UintegerValue (packetSize));
  udpClientServer.SetAttribute ("MaxPackets", UintegerValue (1000000));

  ApplicationContainer clientApps = udpClientServer.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime));

  ApplicationContainer serverApps = udpClientServer.Install (nodes.Get (numNodes - 1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  if (tracing)
    {
      PointToPointHelper pointToPoint;
      pointToPoint.EnablePcapAll ("manet-aodv");
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));

  AnimationInterface anim ("manet-aodv.xml");
  anim.SetMaxPktsPerTraceFile(500000);
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum:  " << i->second.delaySum << "\n";
      std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
    }

  monitor->SerializeToXmlFile("manet-aodv.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}