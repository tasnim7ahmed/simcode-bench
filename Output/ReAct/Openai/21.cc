#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

class Experiment
{
public:
  Experiment ();
  void Run (std::string phyMode, double rssDbm, Gnuplot2dDataset &dataset);
  
private:
  void ConfigureWifi (std::string phyMode, double rssDbm);
  void CreateNodes ();
  void SetMobility (double distance);
  void InstallInternetStack ();
  void InstallApplications ();
  void ReceivePacket (Ptr<Socket> socket);
  void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, uint32_t nPkts, Time pktInterval);

  NodeContainer nodes;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;
  uint32_t packetsReceived;
  uint32_t totalPackets;
};

Experiment::Experiment ()
  : packetsReceived (0),
    totalPackets (100)
{
}

void
Experiment::ConfigureWifi (std::string phyMode, double rssDbm)
{
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue (phyMode),
                               "ControlMode", StringValue ("DsssRate1Mbps"));

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("RxGain", DoubleValue (0));
  wifiPhy.Set ("TxGain", DoubleValue (0));
  wifiPhy.Set ("RxNoiseFigure", DoubleValue (7.0));
  wifiPhy.Set ("CcaEdThreshold", DoubleValue (-62.0));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-62.0));

  Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel> ();
  Ptr<ConstantSpeedPropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel> ();
  channel->SetPropagationDelayModel (delay);

  /* Propagation loss model for received signal strength settings */
  Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
  lossModel->SetReference (1.0, 0.0);
  lossModel->SetPathLossExponent (1.0);
  channel->SetPropagationLossModel (lossModel);

  wifiPhy.SetChannel (channel);

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid ("wifi-experiment");
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));

  devices.Add (wifi.Install (wifiPhy, wifiMac, nodes.Get (0)));

  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));

  devices.Add (wifi.Install (wifiPhy, wifiMac, nodes.Get (1)));

  /* Set the RSS to the specified value by adjusting node distance */
  Ptr<YansWifiPhy> phy = DynamicCast<YansWifiPhy> (devices.Get (0)->GetObject<NetDevice> ()->GetObject<WifiNetDevice> ()->GetPhy ());
  phy->SetTxPowerStart (16);
  phy->SetTxPowerEnd (16);
}

void
Experiment::CreateNodes ()
{
  nodes.Create (2);
}

void
Experiment::SetMobility (double distance)
{
  MobilityHelper mobility;

  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (distance, 0.0, 0.0));

  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (nodes);
}

void
Experiment::InstallInternetStack ()
{
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces = ipv4.Assign (devices);
}

void
Experiment::ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  while ((packet = socket->Recv ()))
    {
      packetsReceived++;
    }
}

void
Experiment::GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, uint32_t nPkts, Time pktInterval)
{
  if (nPkts > 0)
    {
      socket->Send (Create<Packet> (pktSize));
      Simulator::Schedule (pktInterval, &Experiment::GenerateTraffic, this, socket, pktSize, nPkts - 1, pktInterval);
    }
}

void
Experiment::InstallApplications ()
{
  uint16_t port = 5000;
  /* Receiver socket on AP */
  Ptr<Socket> recvSocket = Socket::CreateSocket (nodes.Get (1), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  recvSocket->Bind (local);
  recvSocket->SetRecvCallback (MakeCallback (&Experiment::ReceivePacket, this));

  /* Sender socket on STA */
  Ptr<Socket> source = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());
  InetSocketAddress remote = InetSocketAddress (interfaces.GetAddress (1), port);

  source->Connect (remote);

  Simulator::ScheduleWithContext (source->GetNode ()->GetId (), Seconds (1.0), &Experiment::GenerateTraffic, this, source, 1024, totalPackets, MilliSeconds (10));
}

void
Experiment::Run (std::string phyMode, double rssDbm, Gnuplot2dDataset &dataset)
{
  CreateNodes ();

  /* Calculate ideal distance for desired RSS */
  double txPowerDbm = 16.0;
  double refLossDb = 0.0;
  double pathLossExp = 1.0;
  double referenceDistance = 1.0;
  /* Friis: Pr = Pt - (10*n*log10(d/d0)), rearranged for d */
  double distance = referenceDistance * std::pow (10.0, (txPowerDbm - rssDbm - refLossDb) / (10 * pathLossExp));

  ConfigureWifi (phyMode, rssDbm);
  SetMobility (distance);
  InstallInternetStack ();

  InstallApplications ();

  Simulator::Stop (Seconds (3.0));
  packetsReceived = 0;
  Simulator::Run ();
  Simulator::Destroy ();

  dataset.Add (rssDbm, packetsReceived);
}

int
main (int argc, char *argv[])
{
  std::vector<std::string> phyModes = {
    "DsssRate1Mbps",
    "DsssRate2Mbps",
    "DsssRate5_5Mbps",
    "DsssRate11Mbps"
  };
  std::vector<double> rssDbms = {
    -90, -85, -80, -75, -70, -65, -60, -55, -50
  };

  Gnuplot plot ("wifi-rss-packets.eps");
  plot.SetTitle ("Packets Received vs Received Signal Strength (RSS)");
  plot.SetTerminal ("postscript eps color enhanced");
  plot.SetLegend ("RSS (dBm)", "Packets received");

  for (const std::string &mode : phyModes)
    {
      Gnuplot2dDataset dataset;
      dataset.SetTitle (mode);

      for (double rss : rssDbms)
        {
          Experiment experiment;
          experiment.Run (mode, rss, dataset);
        }
      plot.AddDataset (dataset);
    }

  std::ofstream plotFile ("wifi-rss-packets.plt");
  plot.GenerateOutput (plotFile);
  plotFile.close ();

  return 0;
}