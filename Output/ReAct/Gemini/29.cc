#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMultirateExperiment");

class WifiMultirateExperiment
{
public:
  WifiMultirateExperiment ();
  void Run ();

private:
  void CreateNodes ();
  void CreateDevices ();
  void InstallInternetStack ();
  void CreateApplications ();
  void InstallMobility ();
  void InstallRouting ();
  void EnablePacketReceptionMonitoring ();
  void CalculateThroughput ();
  void GenerateGnuplotFiles ();

  uint32_t m_numNodes = 100;
  double m_simulationTime = 10.0;
  std::string m_rate = "54Mbps";
  std::string m_protocol = "UdpClientServer";
  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
  Ptr<FlowMonitor> m_flowMonitor;
  FlowMonitorHelper m_flowMonitorHelper;
};

WifiMultirateExperiment::WifiMultirateExperiment ()
{
}

void WifiMultirateExperiment::Run ()
{
  CreateNodes ();
  CreateDevices ();
  InstallInternetStack ();
  InstallRouting ();
  CreateApplications ();
  InstallMobility ();
  EnablePacketReceptionMonitoring ();

  Simulator::Stop (Seconds (m_simulationTime));
  Simulator::Run ();

  CalculateThroughput ();
  GenerateGnuplotFiles ();

  Simulator::Destroy ();
}

void WifiMultirateExperiment::CreateNodes ()
{
  NS_LOG_INFO ("Creating " << m_numNodes << " nodes.");
  m_nodes.Create (m_numNodes);
}

void WifiMultirateExperiment::CreateDevices ()
{
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                       StringValue (m_rate));

  m_devices = wifi.Install (wifiPhy, wifiMac, m_nodes);
}

void WifiMultirateExperiment::InstallInternetStack ()
{
  InternetStackHelper internet;
  internet.Install (m_nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  m_interfaces = ipv4.Assign (m_devices);
}

void WifiMultirateExperiment::CreateApplications ()
{
  uint16_t port = 9;

  for (uint32_t i = 0; i < m_numNodes; ++i)
    {
      for (uint32_t j = i + 1; j < m_numNodes; j += (m_numNodes / 5)) {
        if(j >= m_numNodes) break;
          UdpClientHelper client (m_interfaces.GetAddress (j, 0), port);
          client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
          client.SetAttribute ("Interval", TimeValue (Time ("0.01")));
          client.SetAttribute ("PacketSize", UintegerValue (1024));
          ApplicationContainer clientApps = client.Install (m_nodes.Get (i));
          clientApps.Start (Seconds (1.0));
          clientApps.Stop (Seconds (m_simulationTime - 1.0));

          UdpServerHelper server (port);
          ApplicationContainer serverApps = server.Install (m_nodes.Get (j));
          serverApps.Start (Seconds (1.0));
          serverApps.Stop (Seconds (m_simulationTime - 1.0));
      }
    }
}


void WifiMultirateExperiment::InstallMobility ()
{
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("0.0"),
                                 "Y", StringValue ("0.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  mobility.Install (m_nodes);
}

void WifiMultirateExperiment::InstallRouting ()
{
  AodvHelper aodv;
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
}

void WifiMultirateExperiment::EnablePacketReceptionMonitoring ()
{
  m_flowMonitor = m_flowMonitorHelper.InstallAll ();
}

void WifiMultirateExperiment::CalculateThroughput ()
{
  m_flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (m_flowMonitorHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = m_flowMonitor->GetFlowStats ();
  double totalThroughput = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1000000;

      std::cout << "Flow ID: " << i->first  << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Throughput: " << throughput << " Mbps" << std::endl;
      totalThroughput += throughput;
    }
  std::cout << "Total Throughput: " << totalThroughput << " Mbps" << std::endl;
}

void WifiMultirateExperiment::GenerateGnuplotFiles ()
{
    Gnuplot throughputPlot;
    throughputPlot.SetTitle ("Throughput vs. Time");
    throughputPlot.SetLegend ("Time (s)", "Throughput (Mbps)");

    Gnuplot2dDataset dataset;
    dataset.SetTitle ("Flow Throughput");
    dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

    m_flowMonitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (m_flowMonitorHelper.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = m_flowMonitor->GetFlowStats ();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1000000;
      dataset.Add (i->second.timeLastRxPacket.GetSeconds(), throughput);
    }

    throughputPlot.AddDataset (dataset);
    throughputPlot.GenerateOutput ("throughput.plt");

    std::ofstream plotFile ("throughput.plt");
    plotFile << throughputPlot.GetPlotFile ();
    plotFile.close ();
}


int main (int argc, char *argv[])
{
  LogComponentEnable ("WifiMultirateExperiment", LOG_LEVEL_INFO);

  CommandLine cmd;
  cmd.Parse (argc, argv);

  WifiMultirateExperiment experiment;
  experiment.Run ();

  return 0;
}