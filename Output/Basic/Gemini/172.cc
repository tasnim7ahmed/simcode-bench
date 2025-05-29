#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = true;
  double simulationTime = 30;
  double areaSize = 500;
  int numberOfNodes = 10;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable or disable PCAP tracing", enablePcap);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("areaSize", "Size of the simulation area (square)", areaSize);
  cmd.AddValue ("numberOfNodes", "Number of mobile nodes", numberOfNodes);

  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (numberOfNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (aodv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaSize) + "]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaSize) + "]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 0, areaSize, areaSize)));
  mobility.Install (nodes);

  uint16_t port = 9;
  ApplicationContainer apps;
  for (int i = 0; i < numberOfNodes; ++i)
  {
    for (int j = 0; j < numberOfNodes; ++j)
    {
      if (i != j)
      {
        Ptr<Node> source = nodes.Get (i);
        Ptr<Node> destination = nodes.Get (j);
        Ipv4Address destAddr = interfaces.GetAddress (j);

        UdpClientHelper client (destAddr, port);
        client.SetAttribute ("MaxPackets", UintegerValue (1000));
        client.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
        client.SetAttribute ("PacketSize", UintegerValue (1024));

        double startTime = (double)rand() / RAND_MAX * (simulationTime / 2.0);

        apps = client.Install (source);
        apps.Start (Seconds (startTime));
        apps.Stop (Seconds (simulationTime));
      }
    }
  }

  if (enablePcap)
  {
    wifiPhy.EnablePcapAll ("aodv_adhoc");
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double totalPacketsSent = 0;
  double totalPacketsReceived = 0;
  double totalDelay = 0;
  int numFlows = 0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

    totalPacketsSent += i->second.txPackets;
    totalPacketsReceived += i->second.rxPackets;
    totalDelay += i->second.delaySum.GetSeconds();
    numFlows++;
  }

  double packetDeliveryRatio = (totalPacketsSent > 0) ? (totalPacketsReceived / totalPacketsSent) : 0;
  double averageEndToEndDelay = (totalPacketsReceived > 0) ? (totalDelay / totalPacketsReceived) : 0;

  std::cout << "Packet Delivery Ratio: " << packetDeliveryRatio << std::endl;
  std::cout << "Average End-to-End Delay: " << averageEndToEndDelay << " seconds" << std::endl;

  Simulator::Destroy ();
  return 0;
}