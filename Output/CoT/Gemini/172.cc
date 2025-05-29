#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAODV");

int main (int argc, char *argv[])
{
  bool enablePcap = true;
  uint32_t numNodes = 10;
  double simulationTime = 30.0;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable or disable PCAP tracing", enablePcap);
  cmd.AddValue ("numNodes", "Number of nodes", numNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (numNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("250"),
                                 "Y", StringValue ("250"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=250]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 0, 500, 500)));
  mobility.Install (nodes);

  UdpClientServerHelper cbr(1024, 100);
  cbr.SetAttribute ("MaxPackets", UintegerValue (0));

  for (uint32_t i = 0; i < numNodes; ++i)
  {
    for (uint32_t j = i + 1; j < numNodes; ++j)
    {
        Ptr<UniformRandomVariable> startTimeSeconds = CreateObject<UniformRandomVariable>();
        startTimeSeconds->SetAttribute ("Min", DoubleValue(0.0));
        startTimeSeconds->SetAttribute ("Max", DoubleValue(5.0));
        double start_time = startTimeSeconds->GetValue();

        cbr.SetAttribute ("RemoteAddress", AddressValue (interfaces.GetAddress (j)));
        ApplicationContainer clientApps = cbr.Install (nodes.Get (i));
        clientApps.Start (Seconds (start_time));
        clientApps.Stop (Seconds (simulationTime - 1.0));

        Ptr<UniformRandomVariable> startTimeSeconds2 = CreateObject<UniformRandomVariable>();
        startTimeSeconds2->SetAttribute ("Min", DoubleValue(0.0));
        startTimeSeconds2->SetAttribute ("Max", DoubleValue(5.0));
        double start_time2 = startTimeSeconds2->GetValue();

        cbr.SetAttribute ("RemoteAddress", AddressValue (interfaces.GetAddress (i)));
        ApplicationContainer clientApps2 = cbr.Install (nodes.Get (j));
        clientApps2.Start (Seconds (start_time2));
        clientApps2.Stop (Seconds (simulationTime - 1.0));
    }
  }

  if (enablePcap)
  {
    phy.EnablePcapAll ("adhoc-aodv");
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds (simulationTime));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double totalPdr = 0.0;
  double totalDelay = 0.0;
  int numFlows = 0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

    if (t.destinationPort != 9)
    {
      double pdr = 0.0;
      if (i->second.txPackets > 0) {
        pdr = ((double)i->second.rxPackets / (double)i->second.txPackets) * 100;
      }
      double delay = i->second.delaySum.GetSeconds() / i->second.rxPackets;

      totalPdr += pdr;
      totalDelay += delay;
      numFlows++;
    }
  }

  Simulator::Destroy ();

  if (numFlows > 0) {
    std::cout << "Average PDR: " << totalPdr / numFlows << "%" << std::endl;
    std::cout << "Average End-to-End Delay: " << totalDelay / numFlows << " seconds" << std::endl;
  } else {
    std::cout << "No flows found." << std::endl;
  }

  return 0;
}