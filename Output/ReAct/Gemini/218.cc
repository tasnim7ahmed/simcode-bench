#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetDsdvSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  uint32_t numNodes = 10;
  double simulationTime = 10.0;
  double distance = 100.0;

  CommandLine cmd;
  cmd.AddValue ("numNodes", "Number of nodes", numNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (numNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (1),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes);

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      Ptr<ConstantVelocityMobilityModel> mob = nodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
      mob->SetVelocity(Vector(10, 0, 0));
    }

  WifiHelper wifi;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  Ssid ssid = Ssid ("ns-3-ssid");
  wifiMac.SetType ("ns3::AdhocWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  DsdvHelper dsdv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (dsdv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (nodes.Get (numNodes - 1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpClientHelper client (interfaces.GetAddress (numNodes - 1), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (1000000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  if (enablePcap)
    {
      wifiPhy.EnablePcapAll ("vanet-dsdv");
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));

  AnimationInterface anim ("vanet-dsdv.xml");
  for (uint32_t i = 0; i < numNodes; ++i) {
    anim.UpdateNodeDescription (nodes.Get(i), "VANET Node");
    anim.UpdateNodeColor (nodes.Get(i), 255, 0, 0);
  }

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
      std::cout << "  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }

  monitor->SerializeToXmlFile("vanet-dsdv.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}