#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/gnuplot.h"
#include "ns3/applications-module.h"
#include "ns3/packet-socket-address.h"

using namespace ns3;

class Experiment
{
public:
  Experiment (std::string manager, std::string dataRate, std::string outputName);
  void Run ();

private:
  void SetPosition (Ptr<Node> node, Vector position);
  Vector GetPosition (Ptr<Node> node) const;
  void MoveNodes ();
  void ReceivePacket (Ptr<Socket> socket);
  void SetRecvSocket ();
  void ResetCounters ();
  std::string m_manager;
  std::string m_dataRate;
  std::string m_outputName;
  NodeContainer nodes;
  NetDeviceContainer devices;
  Ptr<PacketSocket> sendSocket;
  Ptr<PacketSocket> recvSocket;
  double m_lastRxTime;
  uint64_t m_totalRxBytes;
  uint32_t m_packetsReceived;
  uint32_t m_movementStep;
  Gnuplot m_gnuplot;
  Gnuplot2dDataset m_dataset;

  // Constants
  static constexpr double m_stepDistance = 10.0;
  static constexpr double m_maxDistance = 200.0;
  static constexpr double m_appStart = 1.0;
  static constexpr double m_appStop = 9.0;
  static constexpr double m_moveInterval = 1.0;
  static constexpr double m_simStop = 10.0;
  static const uint16_t m_packetPort = 9;
};

Experiment::Experiment (std::string manager, std::string dataRate, std::string outputName)
  : m_manager (manager),
    m_dataRate (dataRate),
    m_outputName (outputName),
    m_lastRxTime (0.0),
    m_totalRxBytes (0),
    m_packetsReceived (0),
    m_movementStep (0),
    m_gnuplot (outputName + ".png"),
    m_dataset (outputName)
{
  m_gnuplot.SetTerminal ("png");
  m_gnuplot.SetTitle ("Throughput vs. Node Distance (" + manager + " " + dataRate + ")");
  m_gnuplot.SetLegend ("Distance (m)", "Throughput (Mbps)");
}

void
Experiment::SetPosition (Ptr<Node> node, Vector position)
{
  node->GetObject<MobilityModel> ()->SetPosition (position);
}

Vector
Experiment::GetPosition (Ptr<Node> node) const
{
  return node->GetObject<MobilityModel> ()->GetPosition ();
}

void
Experiment::ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> pkt;
  while ((pkt = socket->Recv ()))
    {
      m_totalRxBytes += pkt->GetSize ();
      m_packetsReceived++;
    }
  m_lastRxTime = Simulator::Now ().GetSeconds ();
}

void
Experiment::SetRecvSocket ()
{
  TypeId tid = TypeId::LookupByName ("ns3::PacketSocketFactory");
  recvSocket = DynamicCast<PacketSocket> (Socket::CreateSocket (nodes.Get (1), tid));
  PacketSocketAddress addr;
  addr.SetSingleDevice (devices.Get (1)->GetIfIndex ());
  addr.SetPhysicalAddress (devices.Get (1)->GetAddress ());
  addr.SetProtocol (0);
  recvSocket->Bind (addr);
  recvSocket->SetRecvCallback (MakeCallback (&Experiment::ReceivePacket, this));
}

void
Experiment::MoveNodes ()
{
  double distance = m_stepDistance * m_movementStep;
  if (distance > m_maxDistance)
    return;

  // Move node 1 to next distance
  Vector pos0 = GetPosition (nodes.Get (0));
  Vector newPos1 = Vector (pos0.x + distance, pos0.y, 0.0);
  SetPosition (nodes.Get (1), newPos1);

  // Reset counters
  ResetCounters ();

  // Wait for transmission interval
  Simulator::Schedule (Seconds (m_appStop - m_appStart), [this, distance] {
    double throughput = static_cast<double>(m_totalRxBytes * 8) / (m_appStop - m_appStart) / 1e6;
    m_dataset.Add (distance, throughput);

    m_movementStep++;
    if (m_stepDistance * m_movementStep <= m_maxDistance)
      {
        MoveNodes ();
      }
    else
      {
        m_gnuplot.AddDataset (m_dataset);
        std::ofstream plotFile (m_outputName + ".plt");
        m_gnuplot.GenerateOutput (plotFile);
      }
  });
}

void
Experiment::ResetCounters ()
{
  m_packetsReceived = 0;
  m_totalRxBytes = 0;
  m_lastRxTime = 0.0;
}

void
Experiment::Run ()
{
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager (m_manager, "DataMode", StringValue (m_dataRate),
                                "ControlMode", StringValue (m_dataRate));

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  devices = wifi.Install (wifiPhy, wifiMac, nodes);

  PacketSocketHelper socketHelper;
  socketHelper.Install (nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  SetPosition (nodes.Get (0), Vector (0.0, 0.0, 0.0));
  SetPosition (nodes.Get (1), Vector (0.0, 0.0, 0.0));

  InternetStackHelper internet;
  internet.Install (nodes);

  // Setup OnOff application (sender to receiver using PacketSocket)
  TypeId tid = TypeId::LookupByName ("ns3::PacketSocketFactory");
  sendSocket = DynamicCast<PacketSocket> (Socket::CreateSocket (nodes.Get (0), tid));
  PacketSocketAddress sendAddr;
  sendAddr.SetSingleDevice (devices.Get (0)->GetIfIndex ());
  sendAddr.SetPhysicalAddress (devices.Get (1)->GetAddress ());
  sendAddr.SetProtocol (0);

  ApplicationContainer apps;
  OnOffHelper onoff ("ns3::PacketSocketFactory", Address (sendAddr));
  onoff.SetAttribute ("DataRate", StringValue ("5Mbps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (1400));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (m_appStart)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (m_appStop)));
  apps.Add (onoff.Install (nodes.Get (0)));

  SetRecvSocket ();

  m_movementStep = 0;
  MoveNodes ();

  Simulator::Stop (Seconds (m_simStop));
  Simulator::Run ();
  Simulator::Destroy ();
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  std::string ratesStr = "DsssRate1Mbps DsssRate2Mbps DsssRate5_5Mbps DsssRate11Mbps";
  std::string managersStr = "ns3::ConstantRateWifiManager ns3::AarfWifiManager";
  cmd.AddValue ("datarates", "Space-separated list of wifi datarates", ratesStr);
  cmd.AddValue ("managers", "Space-separated list of wifi managers", managersStr);
  cmd.Parse (argc, argv);

  std::istringstream ratesStream (ratesStr);
  std::istringstream managersStream (managersStr);
  std::vector<std::string> rates;
  std::vector<std::string> managers;
  std::string token;

  while (ratesStream >> token)
    rates.push_back (token);
  while (managersStream >> token)
    managers.push_back (token);

  for (const auto& manager : managers)
    {
      for (const auto& rate : rates)
        {
          std::string expLabel = manager.substr (manager.find_last_of (':') + 1) + "_" + rate;
          Experiment exp (manager, rate, expLabel);
          exp.Run ();
        }
    }
  return 0;
}