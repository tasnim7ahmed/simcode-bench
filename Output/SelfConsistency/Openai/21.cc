#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

class Experiment
{
public:
  Experiment ();
  void Configure (std::string phyMode, double rss, uint32_t packetSize, uint32_t numPackets, double interval);
  void Run ();
  uint32_t GetReceivedPackets () const;

private:
  void ReceivePacket (Ptr<Socket> socket);
  void GenerateTraffic (Ptr<Socket> socket, uint32_t pktCount, uint32_t pktSize, Time pktInterval);
  void SetPosition (Ptr<Node>, double x, double y, double z);

  uint32_t m_receivedPackets;
  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
  std::string m_phyMode;
  double m_rss;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  double m_interval;
};

Experiment::Experiment ()
  : m_receivedPackets (0)
{
}

void
Experiment::Configure (std::string phyMode, double rss, uint32_t packetSize, uint32_t numPackets, double interval)
{
  m_phyMode = phyMode;
  m_rss = rss;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_interval = interval;
}

void
Experiment::SetPosition (Ptr<Node> node, double x, double y, double z)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  if (!mobility)
    {
      Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
      positionAlloc->Add (Vector (x, y, z));
      MobilityHelper mobilityHelper;
      mobilityHelper.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobilityHelper.SetPositionAllocator (positionAlloc);
      NodeContainer nc;
      nc.Add (node);
      mobilityHelper.Install (nc);
    }
  else
    {
      mobility->SetPosition (Vector (x, y, z));
    }
}

void
Experiment::ReceivePacket (Ptr<Socket> socket)
{
  while (Ptr<Packet> packet = socket->Recv ())
    {
      ++m_receivedPackets;
    }
}

void
Experiment::GenerateTraffic (Ptr<Socket> socket, uint32_t pktCount, uint32_t pktSize, Time pktInterval)
{
  if (pktCount > 0)
    {
      socket->Send (Create<Packet> (pktSize));
      Simulator::Schedule (pktInterval, &Experiment::GenerateTraffic, this, socket, pktCount - 1, pktSize, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}

void
Experiment::Run ()
{
  m_nodes.Create (2);

  // Set positions: sender at (0,0,0), receiver at (5,0,0)
  SetPosition (m_nodes.Get (0), 0.0, 0.0, 0.0);
  SetPosition (m_nodes.Get (1), 5.0, 0.0, 0.0);

  // Install Wifi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue (m_phyMode),
                               "ControlMode", StringValue (m_phyMode));

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Set receive power levels and other PHY configs
  wifiPhy.Set ("TxPowerStart", DoubleValue (20.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (20.0));
  wifiPhy.Set ("RxGain", DoubleValue (0));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-80.0));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-80.0));

  // Model based path loss via constant RSS
  wifiPhy.Set ("RxSensitivity", DoubleValue (-100)); // so we don't drop due to hardware

  // Use fixed RSS
  Ptr<YansWifiChannel> channel = DynamicCast<YansWifiChannel> (wifiPhy.GetChannel ());
  for (uint32_t i = 0; i < channel->GetNPropagationLossModels (); ++i)
    {
      channel->RemovePropagationLossModel (channel->GetPropagationLossModel (i));
    }
  Ptr<PropagationLossModel> rssLoss = CreateObject<FixedRssLossModel> ();
  rssLoss->SetAttribute ("Rss", DoubleValue (m_rss));
  channel->AddPropagationLossModel (rssLoss);

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  m_devices = wifi.Install (wifiPhy, wifiMac, m_nodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (m_nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  m_interfaces = ipv4.Assign (m_devices);

  // Create sockets
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (m_nodes.Get (1), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 9);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&Experiment::ReceivePacket, this));

  Ptr<Socket> source = Socket::CreateSocket (m_nodes.Get (0), tid);
  InetSocketAddress remote = InetSocketAddress (m_interfaces.GetAddress (1), 9);
  source->Connect (remote);

  // Start traffic at 1.0s
  Simulator::Schedule (Seconds (1.0), &Experiment::GenerateTraffic, this, source, m_numPackets, m_packetSize, Seconds (m_interval));

  Simulator::Stop (Seconds (1.0 + m_numPackets * m_interval + 1.0));
  Simulator::Run ();
  Simulator::Destroy ();
}

uint32_t
Experiment::GetReceivedPackets () const
{
  return m_receivedPackets;
}

int
main (int argc, char *argv[])
{
  uint32_t numPackets = 100;
  uint32_t packetSize = 1000; // bytes
  double interval = 0.01; // seconds
  std::vector<std::string> phyModes{ "DsssRate1Mbps", "DsssRate2Mbps", "DsssRate5_5Mbps", "DsssRate11Mbps" };
  std::vector<double> rssValues;
  for (double r = -98; r <= -60; r += 2.0) // from -98 dBm to -60 dBm
    {
      rssValues.push_back (r);
    }

  Gnuplot gnuplot ("wifi-rss-vs-throughput.eps");
  gnuplot.SetTitle ("802.11b: RSS vs Received Packets");
  gnuplot.SetLegend ("RSS (dBm)", "Packets Received");

  for (const auto& phyMode : phyModes)
    {
      Gnuplot2dDataset dataset;
      dataset.SetTitle (phyMode);

      for (double rss : rssValues)
        {
          Experiment experiment;
          experiment.Configure (phyMode, rss, packetSize, numPackets, interval);
          experiment.Run ();
          uint32_t received = experiment.GetReceivedPackets ();
          dataset.Add (rss, received);
          std::cout << "Mode: " << phyMode << " RSS: " << rss << " Received: " << received << std::endl;
        }
      gnuplot.AddDataset (dataset);
    }

  std::ofstream plotFile ("wifi-rss-vs-throughput.plt");
  gnuplot.GenerateOutput (plotFile);
  plotFile.close ();

  std::cout << "Plot data generated: wifi-rss-vs-throughput.eps" << std::endl;

  return 0;
}