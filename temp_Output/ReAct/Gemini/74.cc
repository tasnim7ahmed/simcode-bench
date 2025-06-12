#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/csv-helper.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetRoutingComparison");

int main (int argc, char *argv[])
{
  bool enableFlowMonitor = false;
  bool enableMobilityTrace = false;
  std::string protocol = "AODV";
  double simulationTime = 200;
  uint32_t numNodes = 50;
  double maxSpeed = 20;
  double txPowerDbm = 7.5;
  std::string csvFileName = "manet_results.csv";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("enableFlowMonitor", "Enable flow monitor", enableFlowMonitor);
  cmd.AddValue ("enableMobilityTrace", "Enable mobility trace", enableMobilityTrace);
  cmd.AddValue ("protocol", "Routing protocol (DSDV, AODV, OLSR, DSR)", protocol);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("numNodes", "Number of nodes", numNodes);
  cmd.AddValue ("maxSpeed", "Maximum node speed in m/s", maxSpeed);
  cmd.AddValue ("txPowerDbm", "Wifi transmit power in dBm", txPowerDbm);
  cmd.AddValue ("csvFileName", "CSV output file name", csvFileName);

  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (numNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (txPowerDbm));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPowerDbm));

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelay");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1500.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"));
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(maxSpeed) + "]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]");
  mobility.Install (nodes);

  AodvHelper aodv;
  OlsrHelper olsr;
  DsdvHelper dsdv;
  DsrHelper dsr;
  DsrMainHelper dsrMain;
  Ipv4ListRoutingHelper routing;

  if (protocol == "AODV")
    {
      routing.Add (aodv, 10);
    }
  else if (protocol == "OLSR")
    {
      routing.Add (olsr, 10);
    }
  else if (protocol == "DSDV")
    {
      routing.Add (dsdv, 10);
    }
  else if (protocol == "DSR")
    {
      routing.Add (dsrMain, 10);
    }
  else
    {
      std::cerr << "Error: Unsupported routing protocol: " << protocol << std::endl;
      return 1;
    }

  InternetStackHelper internet;
  internet.SetRoutingHelper (routing);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  uint16_t port = 9;
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;
  for (uint32_t i = 0; i < 10; ++i)
    {
      UdpServerHelper echoServer (port);
      serverApps.Add (echoServer.Install (nodes.Get (i)));

      UdpClientHelper echoClient (i.GetAddress (i % numNodes), port);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
      clientApps.Add (echoClient.Install (nodes.Get ((i + 10) % numNodes)));
      port++;
    }

    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (simulationTime));
    clientApps.Start (Seconds (50.0 + (double)rand() / RAND_MAX));
    clientApps.Stop (Seconds (simulationTime));

  if (enableFlowMonitor)
    {
      FlowMonitorHelper flowmon;
      Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

      Simulator::Stop (Seconds (simulationTime));
      Simulator::Run ();

      monitor->CheckForLostPackets ();

      Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
      std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

      std::ofstream out (csvFileName.c_str());
      if (!out.is_open()) {
        std::cerr << "Could not open " << csvFileName << " for writing.  Exiting." << std::endl;
        exit (1);
      }

      out << "FlowId,SrcAddr,DstAddr,SrcPort,DstPort,Protocol,TxPackets,RxPackets,LostPackets,Throughput(bps),Delay(s)" << std::endl;

      for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
        {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          out << i->first << ","
              << t.sourceAddress << ","
              << t.destinationAddress << ","
              << t.sourcePort << ","
              << t.destinationPort << ","
              << (int)t.protocol << ","
              << i->second.txPackets << ","
              << i->second.rxPackets << ","
              << i->second.lostPackets << ","
              << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) << ","
	      << i->second.delaySum.GetSeconds()/i->second.rxPackets << std::endl;
        }

      out.close ();

      monitor->SerializeToXmlFile("manet_flowmon.xml", true, true);
    }
  else
    {
      Simulator::Stop (Seconds (simulationTime));
      Simulator::Run ();
    }

  if (enableMobilityTrace)
    {
      AnimationInterface anim ("manet-animation.xml");
    }

  Simulator::Destroy ();
  return 0;
}