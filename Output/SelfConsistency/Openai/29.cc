/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/aodv-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMultirateExperiment");

class WifiMultirateExperiment
{
public:
  WifiMultirateExperiment ();
  void Configure (int argc, char **argv);
  void Run ();
  void Report (std::string filename);

private:
  void SetupMobility ();
  void SetupWifi ();
  void SetupInternet ();
  void SetupApplications ();
  void CheckThroughput ();

  uint32_t m_nNodes;
  double m_simTime;
  double m_packetInterval;
  uint32_t m_packetSize;
  uint16_t m_numFlows;
  uint16_t m_port;
  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
  std::vector<ApplicationContainer> m_serverApps;
  std::vector<ApplicationContainer> m_clientApps;
  Gnuplot m_gnuplot;
  Ptr<FlowMonitor> m_flowMonitor;
  FlowMonitorHelper m_flowHelper;
  std::string m_phyMode;
  std::string m_dataRate;
  std::string m_trName;
};

WifiMultirateExperiment::WifiMultirateExperiment ()
  : m_nNodes (100),
    m_simTime (60.0),
    m_packetInterval (0.1),
    m_packetSize (1024),
    m_numFlows (10),
    m_port (9),
    m_phyMode ("DsssRate11Mbps"),
    m_dataRate ("ConstantRate"),
    m_trName ("wifi-multirate-experiment")
{
  m_gnuplot.SetTitle ("WiFi Multirate Throughput vs Time");
  m_gnuplot.SetTerminal ("png");
  m_gnuplot.SetLegend ("Time (s)", "Throughput (Mbps)");
}

void
WifiMultirateExperiment::Configure (int argc, char **argv)
{
  CommandLine cmd;
  cmd.AddValue ("nNodes", "Number of Ad Hoc nodes", m_nNodes);
  cmd.AddValue ("simTime", "Simulation time in seconds", m_simTime);
  cmd.AddValue ("packetSize", "Packet size (bytes)", m_packetSize);
  cmd.AddValue ("packetInterval", "Packet interval (s)", m_packetInterval);
  cmd.AddValue ("numFlows", "Number of simultaneous flows", m_numFlows);
  cmd.AddValue ("phyMode", "WiFi PHY mode (e.g., DsssRate1Mbps, DsssRate11Mbps...)", m_phyMode);
  cmd.Parse (argc, argv);
  NS_LOG_INFO ("Configure done.");
}

void
WifiMultirateExperiment::SetupMobility ()
{
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
      "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
      "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
      "PositionAllocator", PointerValue (
          CreateObjectWithAttributes<RandomRectanglePositionAllocator> (
            "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
            "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"))));
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (10.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (10),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.Install (m_nodes);
}

void
WifiMultirateExperiment::SetupWifi ()
{
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager (m_dataRate, "DataMode", StringValue (m_phyMode),
      "ControlMode", StringValue (m_phyMode));
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  m_devices = wifi.Install (wifiPhy, wifiMac, m_nodes);
}

void
WifiMultirateExperiment::SetupInternet ()
{
  InternetStackHelper internet;
  AodvHelper aodv;
  internet.SetRoutingHelper (aodv);
  internet.Install (m_nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  m_interfaces = ipv4.Assign (m_devices);
}

void
WifiMultirateExperiment::SetupApplications ()
{
  uint32_t flowsSetup = 0;
  m_serverApps.resize (m_numFlows);
  m_clientApps.resize (m_numFlows);
  for (uint16_t i = 0; i < m_numFlows; ++i)
    {
      // Randomly choose source and destination nodes, making sure they're different
      uint32_t srcIdx, dstIdx;
      do
        {
          srcIdx = rand () % m_nNodes;
          dstIdx = rand () % m_nNodes;
        }
      while (srcIdx == dstIdx);

      Address sinkAddress (InetSocketAddress (m_interfaces.GetAddress (dstIdx), m_port + i));
      PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), m_port + i));
      m_serverApps[i] = sinkHelper.Install (m_nodes.Get (dstIdx));
      m_serverApps[i].Start (Seconds (0.0));
      m_serverApps[i].Stop (Seconds (m_simTime + 1));

      UdpClientHelper client (m_interfaces.GetAddress (dstIdx), m_port + i);
      client.SetAttribute ("MaxPackets", UintegerValue (uint32_t (m_simTime/m_packetInterval) + 1));
      client.SetAttribute ("Interval", TimeValue (Seconds (m_packetInterval)));
      client.SetAttribute ("PacketSize", UintegerValue (m_packetSize));
      m_clientApps[i] = client.Install (m_nodes.Get (srcIdx));
      m_clientApps[i].Start (Seconds (1.0));
      m_clientApps[i].Stop (Seconds (m_simTime));
      ++flowsSetup;
    }
  NS_LOG_INFO ("Installed " << flowsSetup << " flows.");
}

void
WifiMultirateExperiment::CheckThroughput ()
{
  static Gnuplot2dDataset dataset ("Throughput");
  double throughputSum = 0;
  for (uint32_t i = 0; i < m_numFlows; ++i)
    {
      Ptr<PacketSink> sink = DynamicCast<PacketSink> (m_serverApps[i].Get (0));
      if (sink)
        {
          double bytesReceived = sink->GetTotalRx ();
          double throughput = (bytesReceived * 8.0) / (m_simTime * 1000000.0); // Mbps
          throughputSum += throughput;
        }
    }
  double curTime = Simulator::Now ().GetSeconds ();
  dataset.Add (curTime, throughputSum);
  if (curTime < m_simTime)
    {
      Simulator::Schedule (Seconds (1.0), &WifiMultirateExperiment::CheckThroughput, this);
    }
  else
    {
      m_gnuplot.AddDataset (dataset);
      Report (m_trName + ".plt");
    }
}

void
WifiMultirateExperiment::Report (std::string filename)
{
  // Flow monitor statistics
  if (m_flowMonitor)
    {
      m_flowMonitor->CheckForLostPackets ();
      Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (m_flowHelper.GetClassifier ());
      FlowMonitor::FlowStatsContainer stats = m_flowMonitor->GetFlowStats ();
      std::ofstream out ((m_trName + "-flowmon.txt").c_str ());
      for (auto const &flow : stats)
        {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
          out << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          out << "  Tx Packets: " << flow.second.txPackets << "\n";
          out << "  Rx Packets: " << flow.second.rxPackets << "\n";
          out << "  Throughput: " << (flow.second.rxBytes * 8.0) / (m_simTime * 1000000.0) << " Mbps\n";
        }
      out.close ();
    }
  std::ofstream plotFile (filename.c_str ());
  m_gnuplot.GenerateOutput (plotFile);
  plotFile.close ();
  NS_LOG_UNCOND ("Plot and stats written. Done.");
}

void
WifiMultirateExperiment::Run ()
{
  m_nodes.Create (m_nNodes);
  SetupMobility ();
  SetupWifi ();
  SetupInternet ();
  SetupApplications ();

  m_flowMonitor = m_flowHelper.InstallAll ();

  Simulator::Schedule (Seconds (1.0), &WifiMultirateExperiment::CheckThroughput, this);

  Simulator::Stop (Seconds (m_simTime + 2));
  Simulator::Run ();
  Simulator::Destroy ();
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("WifiMultirateExperiment", LOG_LEVEL_INFO);

  WifiMultirateExperiment exp;
  exp.Configure (argc, argv);
  exp.Run ();

  return 0;
}

/* 
=== Build Instructions ===

Save this file as wifi-multirate-experiment.cc in your ns-3 'scratch' directory.

$ ./waf build
$ ./waf --run "scratch/wifi-multirate-experiment --nNodes=100 --numFlows=10 --simTime=60"

To debug:
  Add "--ns3::LogComponentEnable=WifiMultirateExperiment:level=all" to command line, or use gdb:
$ gdb --args ./build/scratch/wifi-multirate-experiment

Output:
  - Results and plot file will be generated as wifi-multirate-experiment.plt and wifi-multirate-experiment-flowmon.txt

*/