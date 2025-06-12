#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/aodv-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMultirateExperiment");

class WifiMultirateExperiment
{
public:
  WifiMultirateExperiment ();
  void ParseCommandLine (int argc, char **argv);
  void Run ();
private:
  void ConfigureWifi ();
  void SetupMobility ();
  void SetupInternetStack ();
  void SetupApplications ();
  void MonitorRx (Ptr<const Packet> packet, const Address &address);
  void CalculateThroughput ();
  void ConfigureLogging ();
  void SetupGnuplot ();
  void SaveResults ();
  void PrintInstructions ();

  uint32_t m_numNodes;
  double m_simulationTime;
  double m_areaSize;
  uint32_t m_numFlows;
  double m_nodeSpeed;
  std::string m_wifiRate;
  std::string m_routingProtocol;
  std::string m_logLevel;
  std::string m_outputFile;
  uint32_t m_packetSize;
  double m_dataRate;
  Gnuplot m_gnuplot;
  std::vector<uint32_t> m_rxPackets;
  std::vector<uint64_t> m_rxBytes;
  std::vector<double> m_timeStamps;
  std::vector<double> m_throughputTrace;

  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
  ApplicationContainer m_applications;
  uint64_t m_totalRx;

  TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;
};

WifiMultirateExperiment::WifiMultirateExperiment ()
  : m_numNodes (100),
    m_simulationTime (60.0),
    m_areaSize (200.0),
    m_numFlows (10),
    m_nodeSpeed (5.0),
    m_wifiRate ("OfdmRate54Mbps"),
    m_routingProtocol ("AODV"),
    m_logLevel ("info"),
    m_outputFile ("wifi_multirate_throughput.plt"),
    m_packetSize (1024),
    m_dataRate (1e6),
    m_gnuplot ("Throughput vs Time", "Time (s)", "Throughput (Mbps)"),
    m_totalRx (0)
{
}

void
WifiMultirateExperiment::ParseCommandLine (int argc, char **argv)
{
  CommandLine cmd;
  cmd.AddValue ("numNodes", "Number of nodes", m_numNodes);
  cmd.AddValue ("simulationTime", "Simulation time (s)", m_simulationTime);
  cmd.AddValue ("areaSize", "Size of square simulation area (m)", m_areaSize);
  cmd.AddValue ("numFlows", "Number of flows", m_numFlows);
  cmd.AddValue ("nodeSpeed", "Node constant speed (m/s)", m_nodeSpeed);
  cmd.AddValue ("wifiRate", "Wifi DataRate string (e.g., OfdmRate54Mbps)", m_wifiRate);
  cmd.AddValue ("routingProtocol", "Routing protocol (AODV/OLSR)", m_routingProtocol);
  cmd.AddValue ("logLevel", "Log level (debug/info)", m_logLevel);
  cmd.AddValue ("outputFile", "Output Gnuplot filename", m_outputFile);
  cmd.Parse (argc, argv);
}

void
WifiMultirateExperiment::ConfigureLogging ()
{
  if (m_logLevel == "debug")
    {
      LogComponentEnable ("WifiMultirateExperiment", LOG_LEVEL_ALL);
      LogComponentEnable ("UdpClient", LOG_LEVEL_ALL);
      LogComponentEnable ("UdpServer", LOG_LEVEL_ALL);
      LogComponentEnable ("AodvRoutingProtocol", LOG_LEVEL_ALL);
      LogComponentEnable ("OlsrRoutingProtocol", LOG_LEVEL_ALL);
    }
  else if (m_logLevel == "info")
    {
      LogComponentEnable ("WifiMultirateExperiment", LOG_LEVEL_INFO);
    }
}

void
WifiMultirateExperiment::ConfigureWifi ()
{
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);

  WifiMacHelper mac;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (m_wifiRate), "ControlMode", StringValue ("OfdmRate6Mbps"));

  mac.SetType ("ns3::AdhocWifiMac",
               "QosSupported", BooleanValue (true));

  m_devices = wifi.Install (phy, mac, m_nodes);
}

void
WifiMultirateExperiment::SetupMobility ()
{
  MobilityHelper mobility;
  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
  x->SetAttribute ("Min", DoubleValue (0.0));
  x->SetAttribute ("Max", DoubleValue (m_areaSize));

  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                "X", PointerValue (x),
                                "Y", PointerValue (x));
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (m_nodes);

  for (uint32_t i = 0; i < m_nodes.GetN (); ++i)
    {
      Ptr<ConstantVelocityMobilityModel> mob = m_nodes.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      double theta = 2 * M_PI * i / m_nodes.GetN ();
      mob->SetVelocity (Vector (m_nodeSpeed * std::cos (theta), m_nodeSpeed * std::sin (theta), 0.0));
    }
}

void
WifiMultirateExperiment::SetupInternetStack ()
{
  InternetStackHelper stack;
  if (m_routingProtocol == "AODV")
    {
      AodvHelper aodv;
      stack.SetRoutingHelper (aodv);
    }
  else if (m_routingProtocol == "OLSR")
    {
      OlsrHelper olsr;
      stack.SetRoutingHelper (olsr);
    }
  else
    {
      NS_LOG_ERROR ("Unsupported routing protocol. Use AODV or OLSR.");
      exit (1);
    }
  stack.Install (m_nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.0.0");
  m_interfaces = address.Assign (m_devices);
}

void
WifiMultirateExperiment::MonitorRx (Ptr<const Packet> packet, const Address &address)
{
  m_totalRx += packet->GetSize ();
}

void
WifiMultirateExperiment::CalculateThroughput ()
{
  double throughput = (m_totalRx * 8.0 / 1e6) / 1.0; // Mbps, for last interval
  m_timeStamps.push_back (Simulator::Now ().GetSeconds ());
  m_throughputTrace.push_back (throughput);
  m_totalRx = 0;

  if (Simulator::Now ().GetSeconds () + 1.0 < m_simulationTime)
    {
      Simulator::Schedule (Seconds (1.0), &WifiMultirateExperiment::CalculateThroughput, this);
    }
}

void
WifiMultirateExperiment::SetupApplications ()
{
  uint16_t port = 8000;
  for (uint32_t i = 0; i < m_numFlows; ++i)
    {
      uint32_t src = i % m_numNodes;
      uint32_t dst = (i * 13 + 7) % m_numNodes;
      while (dst == src)
        dst = (dst + 1) % m_numNodes;

      UdpServerHelper server (port + i);
      ApplicationContainer serverApp = server.Install (m_nodes.Get (dst));
      serverApp.Start (Seconds (1.0));
      serverApp.Stop (Seconds (m_simulationTime));

      UdpClientHelper client (m_interfaces.GetAddress (dst), port + i);
      client.SetAttribute ("MaxPackets", UintegerValue (10000000));
      client.SetAttribute ("Interval", TimeValue (Seconds (double (m_packetSize * 8) / m_dataRate)));
      client.SetAttribute ("PacketSize", UintegerValue (m_packetSize));
      ApplicationContainer clientApp = client.Install (m_nodes.Get (src));
      clientApp.Start (Seconds (2.0));
      clientApp.Stop (Seconds (m_simulationTime));
      m_applications.Add (clientApp);
      m_applications.Add (serverApp);

      Ptr<Node> dstNode = m_nodes.Get (dst);
      Ptr<Application> app = serverApp.Get (0);
      Ptr<UdpServer> udpServer = DynamicCast<UdpServer> (app);
      if (udpServer)
        {
          udpServer->TraceConnectWithoutContext ("Rx", MakeCallback (&WifiMultirateExperiment::MonitorRx, this));
        }
    }
}

void
WifiMultirateExperiment::SetupGnuplot ()
{
  Gnuplot2dDataset dataset ("Throughput");
  for (uint32_t i = 0; i < m_timeStamps.size (); ++i)
    {
      dataset.Add (m_timeStamps[i], m_throughputTrace[i]);
    }
  dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);
  m_gnuplot.AddDataset (dataset);
}

void
WifiMultirateExperiment::SaveResults ()
{
  SetupGnuplot ();
  std::ofstream plotFile (m_outputFile.c_str ());
  m_gnuplot.GenerateOutput (plotFile);
  plotFile.close ();
  NS_LOG_INFO ("Result plot saved to " << m_outputFile);
}

void
WifiMultirateExperiment::PrintInstructions ()
{
  std::cout << "==== Build ==== " << std::endl;
  std::cout << "  ./waf configure --enable-examples" << std::endl;
  std::cout << "  ./waf build" << std::endl;
  std::cout << "==== Run ====" << std::endl;
  std::cout << "  ./waf --run \"wifi-multirate-experiment [--numNodes=N] [--simulationTime=S] [--routingProtocol=AODV|OLSR] [--logLevel=info|debug]\"" << std::endl;
  std::cout << "==== Debug ====" << std::endl;
  std::cout << "  ./waf --run \"wifi-multirate-experiment --logLevel=debug\"" << std::endl;
}

void
WifiMultirateExperiment::Run ()
{
  PrintInstructions ();
  ConfigureLogging ();

  m_nodes.Create (m_numNodes);
  ConfigureWifi ();
  SetupMobility ();
  SetupInternetStack ();
  SetupApplications ();

  m_totalRx = 0;
  Simulator::Schedule (Seconds (1.0), &WifiMultirateExperiment::CalculateThroughput, this);

  Simulator::Stop (Seconds (m_simulationTime + 1));
  Simulator::Run ();
  Simulator::Destroy ();

  SaveResults ();
}

int
main (int argc, char *argv[])
{
  WifiMultirateExperiment experiment;
  experiment.ParseCommandLine (argc, argv);
  experiment.Run ();
  return 0;
}