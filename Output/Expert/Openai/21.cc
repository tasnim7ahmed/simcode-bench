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
  void Configure (std::string mode, double rss, Gnuplot2dDataset &dataset);

private:
  void Reset ();
  void CreateNodes ();
  void ConfigureWifi (std::string mode, double rss);
  void InstallInternetStack ();
  void InstallApplications ();
  void SetMobility ();
  void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, uint32_t maxPkts, Time pktInterval);
  void RxPacket (Ptr<Socket> socket);

  uint32_t m_rxPackets;
  NodeContainer nodes;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;
};

Experiment::Experiment ()
  : m_rxPackets (0)
{}

void
Experiment::Configure (std::string mode, double rss, Gnuplot2dDataset &dataset)
{
  Reset ();
  CreateNodes ();
  ConfigureWifi (mode, rss);
  InstallInternetStack ();
  SetMobility ();
  InstallApplications ();
  m_rxPackets = 0;

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  dataset.Add (rss, m_rxPackets);

  Simulator::Destroy ();
}

void
Experiment::Reset ()
{
  nodes = NodeContainer ();
  devices = NetDeviceContainer ();
  interfaces = Ipv4InterfaceContainer ();
  m_rxPackets = 0;
}

void
Experiment::CreateNodes ()
{
  nodes.Create (2);
}

void
Experiment::ConfigureWifi (std::string mode, double rss)
{
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue (mode),
                               "ControlMode", StringValue (mode));

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();  
  Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel> ();
  Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
  lossModel->SetReference (1.0, 7.9); // Reference loss for 1m for 2.4GHz
  channel->SetPropagationLossModel (lossModel);
  Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
  channel->SetPropagationDelayModel (delayModel);

  phy.SetChannel (channel);
  phy.Set ("TxPowerStart", DoubleValue (16.0206)); // 40 mW
  phy.Set ("TxPowerEnd", DoubleValue (16.0206));
  phy.Set ("RxSensitivity", DoubleValue (-100.0));
  
  // Apply a controlled fixed RSS by using RSS loss
  Ptr<ConstantPositionPropagationLossModel> fixedLoss = CreateObject<ConstantPositionPropagationLossModel> ();
  fixedLoss->SetLossDb (16.0206 - rss);
  channel->AddPropagationLossModel (fixedLoss);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-80211b");
  mac.SetType ("ns3::AdhocWifiMac", "Ssid", SsidValue (ssid));
  devices = wifi.Install (phy, mac, nodes);
}

void
Experiment::InstallInternetStack ()
{
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces = address.Assign (devices);
}

void
Experiment::SetMobility ()
{
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  Ptr<MobilityModel> m0 = nodes.Get (0)->GetObject<MobilityModel> ();
  Ptr<MobilityModel> m1 = nodes.Get (1)->GetObject<MobilityModel> ();

  m0->SetPosition (Vector (0.0, 0.0, 0.0));
  m1->SetPosition (Vector (5.0, 0.0, 0.0));
}

void
Experiment::InstallApplications ()
{
  uint16_t port = 5000;
  uint32_t packetSize = 1000;
  uint32_t numPackets = 1000;
  Time pktInterval = Seconds (0.005); // 200 pkt/sec

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");

  Ptr<Socket> recvSink = Socket::CreateSocket (nodes.Get (1), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&Experiment::RxPacket, this));

  Ptr<Socket> source = Socket::CreateSocket (nodes.Get (0), tid);
  InetSocketAddress remote = InetSocketAddress (interfaces.GetAddress (1), port);
  source->Connect (remote);

  Simulator::ScheduleWithContext (source->GetNode ()->GetId (), Seconds (1.0),
                                  &Experiment::GenerateTraffic, this, source, packetSize,
                                  numPackets, pktInterval);
}

void
Experiment::GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, uint32_t maxPkts, Time pktInterval)
{
  if (maxPkts > 0)
    {
      Ptr<Packet> packet = Create<Packet> (pktSize);
      socket->Send (packet);
      Simulator::Schedule (pktInterval, &Experiment::GenerateTraffic, this, socket, pktSize, maxPkts - 1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}

void
Experiment::RxPacket (Ptr<Socket> socket)
{
  while (Ptr<Packet> packet = socket->Recv ())
    {
      m_rxPackets++;
    }
}

int
main (int argc, char *argv[])
{
  std::vector<std::string> dataRates = {
    "DsssRate1Mbps",
    "DsssRate2Mbps",
    "DsssRate5_5Mbps",
    "DsssRate11Mbps"
  };

  std::vector<double> rssValues;
  for (double rss = -95; rss <= -40; rss += 5)
    {
      rssValues.push_back (rss);
    }

  Gnuplot plot ("wifi-clear-channel.eps");
  plot.SetTitle ("802.11b WiFi: Packets Received vs. RSS");
  plot.SetTerminal ("postscript eps");
  plot.SetLegend ("RSS (dBm)", "Packets Received");

  for (const auto &rate : dataRates)
    {
      Gnuplot2dDataset dataset;
      dataset.SetTitle (rate);

      for (const auto &rss : rssValues)
        {
          Experiment experiment;
          experiment.Configure (rate, rss, dataset);
        }
      plot.AddDataset (dataset);
    }

  std::ofstream plotFile ("wifi-clear-channel.plt");
  plot.GenerateOutput (plotFile);
  plotFile.close ();

  return 0;
}