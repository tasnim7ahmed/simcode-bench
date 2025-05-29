#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-address.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include <string>
#include <vector>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocWifiExperiment");

class Experiment
{
public:
  Experiment ();
  ~Experiment ();
  void Run (std::string phyMode, std::string wifiManager, Gnuplot2dDataset &dataset);
  void SetPosition (Ptr<Node> node, Vector position);
  Vector GetPosition (Ptr<Node> node) const;
  void MovePosition (Ptr<Node> node, Vector delta);
  void SetupPacketReceive (Ptr<Node> node);
  void ReceivePacket (Ptr<Socket> socket);

  double GetThroughputKbps () const { return m_throughputKbps; }
  void ResetCounters ()
  {
    m_bytesTotal = 0;
    m_throughputKbps = 0;
  }

private:
  uint64_t m_bytesTotal;
  Time m_lastCalcTime;
  double m_throughputKbps;
};

Experiment::Experiment ()
  : m_bytesTotal (0),
    m_lastCalcTime (Seconds (0.0)),
    m_throughputKbps (0.0)
{
}

Experiment::~Experiment ()
{
}

void
Experiment::SetPosition (Ptr<Node> node, Vector position)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  mobility->SetPosition (position);
}

Vector
Experiment::GetPosition (Ptr<Node> node) const
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  return mobility->GetPosition ();
}

void
Experiment::MovePosition (Ptr<Node> node, Vector delta)
{
  Vector pos = GetPosition (node);
  pos.x += delta.x;
  pos.y += delta.y;
  pos.z += delta.z;
  SetPosition (node, pos);
}

void
Experiment::ReceivePacket (Ptr<Socket> socket)
{
  while (Ptr<Packet> packet = socket->Recv ())
    {
      m_bytesTotal += packet->GetSize ();
    }
}

void
Experiment::SetupPacketReceive (Ptr<Node> node)
{
  TypeId tid = TypeId::LookupByName ("ns3::PacketSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket (node, tid);

  PacketSocketAddress socketAddr;
  socketAddr.SetSingleDevice (0);
  socketAddr.SetPhysicalAddress (Mac48Address::GetBroadcast ());
  socketAddr.SetProtocol (0x0800);

  sink->Bind (socketAddr);
  sink->SetRecvCallback (MakeCallback (&Experiment::ReceivePacket, this));
}

void
Experiment::Run (std::string phyMode, std::string wifiManager, Gnuplot2dDataset &dataset)
{
  m_bytesTotal = 0;
  m_throughputKbps = 0.0;
  m_lastCalcTime = Seconds (0.0);

  // Nodes and helpers
  NodeContainer nodes;
  nodes.Create (2);

  // Wifi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager (wifiManager, "DataMode", StringValue (phyMode), "ControlMode", StringValue ("DsssRate1Mbps"));

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  // Packet socket for sending/receiving L2 packets (ad hoc)
  PacketSocketHelper packetSocket;
  packetSocket.Install (nodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
  SetPosition (nodes.Get (0), Vector (0.0, 0.0, 0.0));
  SetPosition (nodes.Get (1), Vector (50.0, 0.0, 0.0)); // Start 50m away

  // Application: OnOff to generate traffic
  PacketSocketAddress socketAddr;
  socketAddr.SetSingleDevice (devices.Get (1)->GetIfIndex ());
  socketAddr.SetPhysicalAddress (Mac48Address::ConvertFrom (devices.Get (1)->GetAddress ()));
  socketAddr.SetProtocol (0x0800);

  OnOffHelper onoff ("ns3::PacketSocketFactory", Address (socketAddr));
  onoff.SetAttribute ("PacketSize", UintegerValue (1400));
  onoff.SetAttribute ("DataRate", StringValue ("5Mbps"));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (11.0))); // Duration = 10s

  ApplicationContainer app = onoff.Install (nodes.Get (0));

  // Receive socket
  SetupPacketReceive (nodes.Get (1));

  // Throughput monitor: at each second log current throughput to dataset
  // Throughput is measured every second based on the accumulated received bytes
  const double totalTime = 12.0;
  for (double t = 1.0; t <= 11.0; t += 1.0)
    {
      Simulator::Schedule (Seconds (t), [this, t, &dataset] {
        double throughputKbps = (m_bytesTotal * 8.0) / 1000.0; // Bits to kilobits
        dataset.Add (t, throughputKbps);
        m_throughputKbps = throughputKbps;
        m_bytesTotal = 0; // reset counter for next interval
      });
    }

  Simulator::Stop (Seconds (totalTime));
  Simulator::Run ();
  Simulator::Destroy ();
}

// --- Main function ---

int
main (int argc, char *argv[])
{
  LogComponentEnable ("AdhocWifiExperiment", LOG_LEVEL_INFO);

  std::string outputPrefix = "adhoc-wifi-experiment";
  CommandLine cmd;
  cmd.AddValue ("outputPrefix", "Prefix for gnuplot output files", outputPrefix);
  cmd.Parse (argc, argv);

  // Define WiFi rates and managers to test
  std::vector<std::string> phyModes { "DsssRate1Mbps", "DsssRate11Mbps" };
  std::vector<std::string> managers =
    { "ns3::ConstantRateWifiManager", "ns3::AarfWifiManager", "ns3::ArfWifiManager" };

  for (const auto& phyMode : phyModes)
    {
      for (const auto& wifiManager : managers)
        {
          NS_LOG_INFO ("Running experiment with Wi-Fi mode " << phyMode << " and manager " << wifiManager);

          // Prepare gnuplot dataset/file
          std::ostringstream plotTitle;
          plotTitle << "Throughput for " << phyMode << " and " << wifiManager;

          std::ostringstream dataFileName;
          dataFileName << outputPrefix << "-" << phyMode << "-" << wifiManager << ".dat";

          std::ostringstream plotFileName;
          plotFileName << outputPrefix << "-" << phyMode << "-" << wifiManager << ".plt";

          Gnuplot gnuplot (dataFileName.str());
          gnuplot.SetTitle (plotTitle.str());
          gnuplot.SetTerminal ("png");
          gnuplot.SetLegend ("Time (s)", "Throughput (kbps)");

          Gnuplot2dDataset dataset;
          dataset.SetTitle ("Throughput");
          dataset.SetStyle (Gnuplot2dDataset::LINES);

          // Run experiment
          Experiment experiment;
          experiment.Run (phyMode, wifiManager, dataset);

          // Add dataset, write the files
          gnuplot.AddDataset (dataset);

          std::ofstream plotFile (plotFileName.str());
          gnuplot.GenerateOutput (plotFile);
          plotFile.close ();
        }
    }

  return 0;
}