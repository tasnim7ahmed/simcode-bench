#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
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
  void InstallMobility ();
  void ConfigureWifiPhy ();
  void ConfigureWifiMac ();
  void SetUpFlowMonitor ();
  void CreateGnuplot ();
  void ReceivePacket (Ptr<const Packet> p, const Address &addr);

  uint32_t m_numNodes;
  double m_simulationTime;
  std::string m_rate;
  std::string m_phyMode;
  bool m_verbose;
  bool m_tracing;
  bool m_animation;
  std::string m_experimentName;
  std::string m_prefixFilename;

  NodeContainer m_nodes;
  WifiHelper m_wifiHelper;
  YansWifiPhyHelper m_phyHelper;
  YansWifiChannelHelper m_channelHelper;
  WifiMacHelper m_macHelper;
  NetDeviceContainer m_devices;
  MobilityHelper m_mobility;
  FlowMonitorHelper m_flowmonHelper;
  Ptr<FlowMonitor> m_flowmon;

  Gnuplot m_gnuplot;
  Gnuplot2dDataset m_throughputDataset;
  std::ofstream m_throughputFile;
};

WifiMultirateExperiment::WifiMultirateExperiment ()
  : m_numNodes (100),
    m_simulationTime (10.0),
    m_rate ("6Mbps"),
    m_phyMode ("DsssRate11Mbps"),
    m_verbose (false),
    m_tracing (false),
    m_animation (false),
    m_experimentName ("wifi-multirate"),
    m_prefixFilename ("wifi-multirate")
{
}

WifiMultirateExperiment::~WifiMultirateExperiment ()
{
  m_throughputFile.close();
}

void
WifiMultirateExperiment::Run (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.AddValue ("numNodes", "Number of nodes", m_numNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", m_simulationTime);
  cmd.AddValue ("rate", "CBR traffic rate (e.g., \"6Mbps\")", m_rate);
  cmd.AddValue ("phyMode", "Wifi Phy mode", m_phyMode);
  cmd.AddValue ("verbose", "Tell application to log if true", m_verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", m_tracing);
  cmd.AddValue ("animation", "Enable NetAnim animation", m_animation);
  cmd.AddValue ("experimentName", "Experiment name", m_experimentName);
  cmd.AddValue ("prefixFilename", "Prefix of output trace files", m_prefixFilename);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                       StringValue (m_phyMode));

  CreateNodes ();
  CreateDevices ();
  InstallInternetStack ();
  InstallApplications ();
  InstallMobility ();
  ConfigureWifiPhy ();
  ConfigureWifiMac ();
  SetUpFlowMonitor ();
  CreateGnuplot ();

  if (m_tracing)
    {
      std::string traceFilename = m_prefixFilename + ".pcap";
      m_phyHelper.EnablePcapAll (traceFilename);
    }

  if (m_animation)
  {
    AnimationInterface anim (m_prefixFilename + ".xml");
  }
  
  Simulator::Stop (Seconds (m_simulationTime));

  Simulator::Run ();

  m_flowmon->SerializeToXmlFile((m_experimentName + ".flowmon").c_str(), false, true);

  Simulator::Destroy ();
}

void
WifiMultirateExperiment::CreateNodes ()
{
  NS_LOG_INFO ("Creating " << (unsigned)m_numNodes << " nodes.");
  m_nodes.Create (m_numNodes);
}

void
WifiMultirateExperiment::CreateDevices ()
{
  m_wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211b);

  m_channelHelper = YansWifiChannelHelper::Default ();
  m_phyHelper = YansWifiPhyHelper::Default ();
  m_phyHelper.SetChannel (m_channelHelper.Create ());

  m_macHelper = WifiMacHelper::Default ();
  m_macHelper.SetType (WifiMacHelper::ADHOC,
                       "Ssid", SsidValue (Ssid ("ns-3-adhoc-wifi")));

  m_devices = m_wifiHelper.Install (m_phyHelper, m_macHelper, m_nodes);
}

void
WifiMultirateExperiment::InstallInternetStack ()
{
  OlsrHelper olsr;
  InternetStackHelper stack;
  stack.Install (m_nodes);
  olsr.Install (m_nodes.Get (0));
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (m_devices);
}

void
WifiMultirateExperiment::InstallApplications ()
{
  uint16_t port = 9;  // Discard port (RFC 863)

  for (uint32_t i = 0; i < m_numNodes; ++i) {
      for (uint32_t j = i + 1; j < m_numNodes; ++j) {
        UdpClientHelper client (m_nodes.GetAddress (j,0), port);
        client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
        client.SetAttribute ("Interval", TimeValue (Time ("0.001"))); // packets/sec
        client.SetAttribute ("PacketSize", UintegerValue (1024));
        ApplicationContainer clientApps = client.Install (m_nodes.Get (i));
        clientApps.Start (Seconds (1.0));
        clientApps.Stop (Seconds (m_simulationTime - 1.0));
      }
  }

  Packet::EnablePrinting ();
}

void
WifiMultirateExperiment::InstallMobility ()
{
  m_mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                   "X", StringValue ("100.0"),
                                   "Y", StringValue ("100.0"),
                                   "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=30]"));
  m_mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Mode", StringValue ("Time"),
                               "Time", StringValue ("1s"),
                               "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                               "Bounds", StringValue ("100x100"));
  m_mobility.Install (m_nodes);
}

void
WifiMultirateExperiment::ConfigureWifiPhy ()
{
  m_phyHelper.Set ("ShortGuardEnabled", BooleanValue (true));
  m_wifiHelper.SetRemoteStationManager ("ns3::ArfWifiManager");
}

void
WifiMultirateExperiment::ConfigureWifiMac ()
{
}

void
WifiMultirateExperiment::SetUpFlowMonitor ()
{
  m_flowmonHelper.SetMonitorAttribute ("MaxPacketsPerFlow", UintegerValue (100000));
  m_flowmon = m_flowmonHelper.InstallAll ();

  m_throughputFile.open((m_experimentName + ".dat").c_str());
}

void
WifiMultirateExperiment::CreateGnuplot ()
{
}

void
WifiMultirateExperiment::ReceivePacket (Ptr<const Packet> p, const Address &addr)
{
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("WifiMultirateExperiment", LOG_LEVEL_INFO);

  WifiMultirateExperiment experiment;
  experiment.Run (argc, argv);

  return 0;
}