#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMultirateExperiment");

class WifiMultirateExperiment
{
public:
  WifiMultirateExperiment ();
  ~WifiMultirateExperiment ();
  void Run (int argc, char *argv[]);

private:
  void CreateNodes ();
  void CreateDevices ();
  void InstallInternetStack ();
  void InstallApplications ();
  void SetupMobility ();
  void ReceivePacket (Ptr<Socket> socket);
  void CalculateThroughput ();
  void CreateGnuplotFiles ();

  uint32_t m_numNodes = 100;
  double m_simulationTime = 10.0;
  std::string m_rate = "OfdmRate6Mbps";
  double m_txDistance = 10.0;
  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
  ApplicationContainer m_sinkApps;
  ApplicationContainer m_clientApps;
  FlowMonitorHelper m_flowmonHelper;
  Ptr<FlowMonitor> m_flowmon;
  std::string m_prefixAvg;
  std::string m_prefixPer;
  std::string m_gnuplotFileName;
  std::string m_graphicsFileName;
};

WifiMultirateExperiment::WifiMultirateExperiment ()
  : m_prefixAvg ("wifi-multirate-experiment-avg"),
    m_prefixPer ("wifi-multirate-experiment-per"),
    m_gnuplotFileName ("wifi-multirate-experiment.plt"),
    m_graphicsFileName ("wifi-multirate-experiment.png")
{
}

WifiMultirateExperiment::~WifiMultirateExperiment ()
{
}

void
WifiMultirateExperiment::Run (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.AddValue ("numNodes", "Number of nodes", m_numNodes);
  cmd.AddValue ("simulationTime", "Simulation time (s)", m_simulationTime);
  cmd.AddValue ("rate", "Wifi data rate", m_rate);
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  CreateNodes ();
  CreateDevices ();
  InstallInternetStack ();
  InstallApplications ();
  SetupMobility ();

  Simulator::Stop (Seconds (m_simulationTime));

  m_flowmon = m_flowmonHelper.InstallAll ();

  Simulator::Run ();

  CalculateThroughput ();
  CreateGnuplotFiles ();

  Simulator::Destroy ();
}

void
WifiMultirateExperiment::CreateNodes ()
{
  m_nodes.Create (m_numNodes);
}

void
WifiMultirateExperiment::CreateDevices ()
{
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-adhoc");
  mac.SetType ("ns3::AdhocWifiMac",
               "Ssid", SsidValue (ssid));

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue (m_rate),
                                "ControlMode", StringValue ("OfdmRate6Mbps"));

  m_devices = wifi.Install (phy, mac, m_nodes);
}

void
WifiMultirateExperiment::InstallInternetStack ()
{
  OlsrHelper olsr;
  InternetStackHelper stack;
  stack.SetRoutingHelper (olsr);
  stack.Install (m_nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  m_interfaces = address.Assign (m_devices);
}

void
WifiMultirateExperiment::InstallApplications ()
{
  uint16_t port = 9;

  UdpServerHelper server (port);
  m_sinkApps = server.Install (m_nodes.Get (m_numNodes - 1));
  m_sinkApps.Start (Seconds (1.0));
  m_sinkApps.Stop (Seconds (m_simulationTime));

  UdpClientHelper client (m_interfaces.GetAddress (m_numNodes - 1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Time ("0.01")));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  m_clientApps = client.Install (m_nodes.Get (0));
  m_clientApps.Start (Seconds (2.0));
  m_clientApps.Stop (Seconds (m_simulationTime));
}

void
WifiMultirateExperiment::SetupMobility ()
{
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (m_txDistance),
                                 "DeltaY", DoubleValue (m_txDistance),
                                 "GridWidth", UintegerValue (10),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (m_nodes);
}

void
WifiMultirateExperiment::ReceivePacket (Ptr<Socket> socket)
{
  Address senderAddress;
  Ptr<Packet> packet = socket->RecvFrom (senderAddress);
  std::cout << Simulator::Now ().GetSeconds () << " " << packet->GetSize () << std::endl;
}

void
WifiMultirateExperiment::CalculateThroughput ()
{
  m_flowmon->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (m_flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = m_flowmon->GetFlowStats ();
  double totalRxBytes = 0.0;
  double totalTime = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets:   " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps\n";

      totalRxBytes += i->second.rxBytes;
      totalTime = std::max (totalTime, i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ());
    }

  std::cout << "Average throughput: " << totalRxBytes * 8.0 / totalTime / 1000000 << " Mbps\n";

  std::ofstream out (m_prefixAvg + ".dat");
  out << Simulator::Now ().GetSeconds () << " " << totalRxBytes * 8.0 / totalTime / 1000000 << std::endl;
  out.close ();

  std::ofstream outPer (m_prefixPer + ".dat");
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  {
      outPer << i->first << " " << (double) i->second.lostPackets / i->second.txPackets * 100 << std::endl;
  }
  outPer.close ();
}

void
WifiMultirateExperiment::CreateGnuplotFiles ()
{
  Gnuplot gnuplot (m_gnuplotFileName);
  gnuplot.SetTitle ("Wifi Multirate Experiment");
  gnuplot.Setxlabel ("Time (s)");
  gnuplot.Setylabel ("Throughput (Mbps)");
  gnuplot.AddDataset ((m_prefixAvg + ".dat"), "Average Throughput");
  gnuplot.GenerateOutput (m_graphicsFileName);
}

int
main (int argc, char *argv[])
{
  WifiMultirateExperiment experiment;
  experiment.Run (argc, argv);
  return 0;
}