#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/mobility-module.h>
#include <ns3/wifi-module.h>
#include <ns3/internet-module.h>
#include <ns3/applications-module.h>
#include <ns3/gnuplot.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ErrorModelComparison");

class ErrorModelComparison
{
public:
  ErrorModelComparison ();
  void Run (uint32_t frameSize, const std::string& outputPlotFileName);

private:
  void SetupSimulation (WifiPhyStandard standard, WifiMode mode, double snr, uint32_t frameSize);
  void SendPacket (Ptr<Socket> socket, uint32_t size);
  void ReceivePacket (Ptr<Socket> socket);
  uint32_t m_receivedPackets;
};

ErrorModelComparison::ErrorModelComparison ()
  : m_receivedPackets (0)
{
}

void
ErrorModelComparison::Run (uint32_t frameSize, const std::string& outputPlotFileName)
{
  WifiPhyStandard phyStandard = WIFI_PHY_STANDARD_80211a;
  Gnuplot plot (outputPlotFileName);
  plot.SetTitle ("Frame Success Rate vs SNR");
  plot.SetXTitle ("SNR (dB)");
  plot.SetYTitle ("Frame Success Rate");

  std::vector<std::pair<std::string, std::string>> errorModels =
  {
    {"NistErrorRateModel", "NIST"},
    {"YansErrorRateModel", "YANS"},
    {"TableBasedErrorRateModel", "Table"}
  };

  std::vector<WifiMode> ofdmModes;
  ofdmModes.push_back (WifiPhy::GetOfdmRate6Mbps ());
  ofdmModes.push_back (WifiPhy::GetOfdmRate9Mbps ());
  ofdmModes.push_back (WifiPhy::GetOfdmRate12Mbps ());
  ofdmModes.push_back (WifiPhy::GetOfdmRate18Mbps ());
  ofdmModes.push_back (WifiPhy::GetOfdmRate24Mbps ());
  ofdmModes.push_back (WifiPhy::GetOfdmRate36Mbps ());
  ofdmModes.push_back (WifiPhy::GetOfdmRate48Mbps ());
  ofdmModes.push_back (WifiPhy::GetOfdmRate54Mbps ());

  for (auto& modelPair : errorModels)
    {
      Gnuplot2dDataset dataset (modelPair.second);
      dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

      Config::SetDefault ("ns3::WifiRemoteStationManager::ErrorRateModel", StringValue (modelPair.first));

      for (double snr = -5; snr <= 30; snr += 2.5)
        {
          m_receivedPackets = 0;
          SetupSimulation (phyStandard, ofdmModes[0], snr, frameSize); // All modes same here

          Simulator::Stop (Seconds (1.0));
          Simulator::Run ();

          double fsr = static_cast<double> (m_receivedPackets) / 100.0;
          dataset.Add (snr, fsr);
          Simulator::Destroy ();
        }

      plot.AddDataset (dataset);
    }

  std::ofstream plotFile (outputPlotFileName.c_str ());
  plot.GenerateOutput (plotFile);
  plotFile.close ();
}

void
ErrorModelComparison::SetupSimulation (WifiPhyStandard standard, WifiMode mode, double snr, uint32_t frameSize)
{
  NodeContainer nodes;
  nodes.Create (2);

  YansWifiPhyHelper phy;
  WifiHelper wifi;
  wifi.SetStandard (standard);

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", WifiModeValue (mode),
                                "ControlMode", WifiModeValue (mode));

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  mac.SetType ("ns3::AdhocWifiMac");

  phy.SetChannel ("ns3::YansWifiChannel");
  phy.Set ("TxPowerStart", DoubleValue (10.0));
  phy.Set ("TxPowerEnd", DoubleValue (10.0));
  phy.Set ("RxGain", DoubleValue (0.0));
  phy.Set ("CcaMode1Threshold", DoubleValue (-79.0));
  phy.Set ("EnergyDetectionThreshold", DoubleValue (-79.0 + 3.0));
  phy.Set ("SnrPerSymbolMode", BooleanValue (true));

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice> (devices.Get (0));
  if (device)
    {
      device->GetPhy ()->SetAttribute ("Frequency", UintegerValue (5180));
      device->GetPhy ()->SetAttribute ("ChannelWidth", UintegerValue (20));
    }

  device = DynamicCast<WifiNetDevice> (devices.Get (1));
  if (device)
    {
      device->GetPhy ()->SetAttribute ("Frequency", UintegerValue (5180));
      device->GetPhy ()->SetAttribute ("ChannelWidth", UintegerValue (20));
    }

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (nodes.Get (1), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ErrorModelComparison::ReceivePacket, this));

  Ptr<Socket> source = Socket::CreateSocket (nodes.Get (0), tid);
  InetSocketAddress remote = InetSocketAddress (interfaces.GetAddress (1), 80);
  source->Connect (remote);

  for (int i = 0; i < 100; ++i)
    {
      Simulator::Schedule (Seconds (0.001 * i), &ErrorModelComparison::SendPacket, this, source, frameSize);
    }
}

void
ErrorModelComparison::SendPacket (Ptr<Socket> socket, uint32_t size)
{
  Ptr<Packet> packet = Create<Packet> (size);
  socket->Send (packet);
}

void
ErrorModelComparison::ReceivePacket (Ptr<Socket> socket)
{
  NS_ASSERT (socket == socket);
  m_receivedPackets++;
}

int
main (int argc, char *argv[])
{
  uint32_t frameSize = 1472;
  std::string outputPlotFileName = "error-model-comparison.plot";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("frameSize", "Size of each frame in bytes", frameSize);
  cmd.AddValue ("outputPlotFile", "Output plot file name", outputPlotFileName);
  cmd.Parse (argc, argv);

  ErrorModelComparison sim;
  sim.Run (frameSize, outputPlotFileName);

  return 0;
}