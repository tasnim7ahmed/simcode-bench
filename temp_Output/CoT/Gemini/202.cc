#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WSN Simulation");

int main (int argc, char *argv[])
{
  bool tracing = false;
  double simulationTime = 10; // seconds
  std::string phyMode ("OfdmRate6Mbps");

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("time", "Simulation time (seconds)", simulationTime);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (6);

  // Set up channel
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrWpanDevices = lrWpanHelper.Install (nodes);

  // Install the MAC layer protocol
  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install (lrWpanDevices);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (sixLowPanDevices);

  // Configure positions of the nodes in a circle
  double radius = 5.0;
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("0.0"),
                                 "Y", StringValue ("0.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=5.0]"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  for(uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<MobilityModel> mob = nodes.Get(i)->GetObject<MobilityModel>();
    Vector pos(radius * cos(2.0 * M_PI * i / nodes.GetN()), radius * sin(2.0 * M_PI * i / nodes.GetN()), 0.0);
    mob->SetPosition(pos);
  }

  // Setup UDP echo client on all nodes except node 0 (sink)
  UdpEchoClientHelper echoClientHelper (interfaces.GetAddress (0, 1), 9);
  echoClientHelper.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClientHelper.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClientHelper.SetAttribute ("PacketSize", UintegerValue (100));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < nodes.GetN (); ++i)
    {
      clientApps.Add (echoClientHelper.Install (nodes.Get (i)));
    }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  // Setup UDP echo server on node 0 (sink)
  UdpEchoServerHelper echoServerHelper (9);
  ApplicationContainer serverApps = echoServerHelper.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Enable Tracing
  if (tracing)
    {
      LrWpanPhyHelper phyHelper;
      phyHelper.EnablePcapAll("wpan-simple");
    }

  // Set up NetAnim
  AnimationInterface anim ("wpan-simple.xml");
  anim.SetMaxPktsPerTraceFile(10000000);

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Packet Delivery Ratio: " << ((double)i->second.rxPackets / (double)i->second.txPackets) << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    }

  Simulator::Destroy ();
  return 0;
}