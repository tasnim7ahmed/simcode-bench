#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DsdvVanetSimulation");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;
  uint32_t nNodes = 10;
  double simulationTime = 10.0;
  double distance = 50.0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("nNodes", "Number of nodes", nNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance between nodes", distance);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (nNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (nNodes),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes);

  for (uint32_t i = 0; i < nNodes; ++i)
    {
      Ptr<ConstantVelocityMobilityModel> mobilityModel = nodes.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      if (i % 2 == 0)
        {
          mobilityModel->SetVelocity (Vector (10, 0, 0));
        }
      else
        {
          mobilityModel->SetVelocity (Vector (-10, 0, 0));
        }
    }

  WifiHelper wifi;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::AdhocWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  DsdvHelper dsdv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (dsdv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (nodes.Get (nNodes - 1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpClientHelper client (interfaces.GetAddress (nNodes - 1), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  if (tracing)
    {
      phy.EnablePcapAll ("DsdvVanetSimulation");
    }

  AnimationInterface anim ("DsdvVanetSimulation.xml");
    for (uint32_t i = 0; i < nNodes; ++i) {
        anim.UpdateNodeDescription(nodes.Get(i), "VANET Node");
        anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0);
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));
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
      std::cout << "  Lost Packets:   " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum:   " << i->second.delaySum << "\n";

    }

  Simulator::Destroy ();
  return 0;
}