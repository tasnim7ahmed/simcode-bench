#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

class Experiment : public Object
{
public:
  static TypeId GetTypeId (void);
  Experiment ();
  virtual ~Experiment ();

  void SetupSimulation (std::string txMode, double distance);
  void StartApplication (void);
  void StopApplication (void);

  void PacketRx (Ptr<const Packet> packet, const Address &from);

private:
  NodeContainer m_apNode;
  NodeContainer m_staNode;
  NetDeviceContainer m_apDev;
  NetDeviceContainer m_staDev;
  Ipv4InterfaceContainer m_apInterface;
  Ipv4InterfaceContainer m_staInterface;
  uint32_t m_packetsReceived;
};

NS_OBJECT_ENSURE_REGISTERED (Experiment);

TypeId
Experiment::GetTypeId (void)
{
  static TypeId tid = TypeId ("Experiment")
    .SetParent<Object> ()
    .AddConstructor<Experiment> ();
  return tid;
}

Experiment::Experiment ()
{
  m_packetsReceived = 0;
}

Experiment::~Experiment ()
{
}

void
Experiment::PacketRx (Ptr<const Packet> packet, const Address &from)
{
  m_packetsReceived++;
}

void
Experiment::SetupSimulation (std::string txMode, double distance)
{
  m_apNode.Create (1);
  m_staNode.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (txMode));

  Ssid ssid = Ssid ("wifi-experiment");
  WifiMacHelper mac;
  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  m_apDev = wifi.Install (phy, mac, m_apNode);

  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));
  m_staDev = wifi.Install (phy, mac, m_staNode);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // AP
  positionAlloc->Add (Vector (distance, 0.0, 0.0)); // STA
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (m_apNode);
  mobility.Install (m_staNode);

  InternetStackHelper stack;
  stack.Install (m_apNode);
  stack.Install (m_staNode);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  m_apInterface = address.Assign (m_apDev);
  m_staInterface = address.Assign (m_staDev);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (m_staNode.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (m_staInterface.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (m_apNode.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Rx",
                   MakeCallback (&Experiment::PacketRx, this));
}

void
Experiment::StartApplication (void)
{
}

void
Experiment::StopApplication (void)
{
}

int main (int argc, char *argv[])
{
  std::vector<std::string> txModes = {
    "DsssRate1Mbps",
    "DsssRate2Mbps",
    "DsssRate5_5Mbps",
    "DsssRate11Mbps"
  };

  std::vector<double> distances = {1.0, 5.0, 10.0, 20.0, 30.0, 40.0, 50.0};

  Gnuplot2dDataset dataset;
  Gnuplot gnuplot = Gnuplot ("results.eps");
  gnuplot.SetTerminal ("postscript eps color enhanced");
  gnuplot.SetTitle ("RSS vs Packets Received");
  gnuplot.SetLegend ("Distance (m)", "Packets Received");

  for (auto mode : txModes)
    {
      Gnuplot2dDataset data;
      data.SetStyle (Gnuplot2dDataset::LINES_POINTS);
      data.SetTitle (mode);

      for (auto dist : distances)
        {
          double rss = -30 - 20 * log10 (dist); // Simple RSS model

          Experiment experiment;
          experiment.SetupSimulation (mode, dist);

          Simulator::Run ();
          Simulator::Destroy ();

          data.Add (rss, experiment.m_packetsReceived);
        }

      gnuplot.AddDataset (data);
    }

  std::ofstream plotFile ("experiment.plot");
  gnuplot.GenerateOutput (plotFile);
  plotFile.close ();

  return 0;
}