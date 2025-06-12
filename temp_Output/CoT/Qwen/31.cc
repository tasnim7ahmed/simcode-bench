#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EhtErrorModelComparison");

class EhtErrorModelTest : public Object
{
public:
  static TypeId GetTypeId (void);
  EhtErrorModelTest ();
  virtual ~EhtErrorModelTest ();

  void RunSimulation ();

private:
  void SetupSimulation ();
  void SendPacket ();
  void CalculateFsr ();
  void GeneratePlots ();

  uint32_t m_frameSize;
  double m_snrStart;
  double m_snrEnd;
  uint32_t m_snrSteps;
  uint32_t m_numMcsValues;
  std::vector<std::vector<double>> m_fsrNist;
  std::vector<std::vector<double>> m_fsrYans;
  std::vector<std::vector<double>> m_fsrTable;
};

TypeId
EhtErrorModelTest::GetTypeId (void)
{
  static TypeId tid = TypeId ("EhtErrorModelTest")
    .SetParent<Object> ()
    .AddConstructor<EhtErrorModelTest> ()
    .AddAttribute ("FrameSize", "The size of the transmitted frames in bytes.",
                   UintegerValue (1000),
                   MakeUintegerAccessor (&EhtErrorModelTest::m_frameSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("SnrStart", "Starting SNR value in dB.",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&EhtErrorModelTest::m_snrStart),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("SnrEnd", "Ending SNR value in dB.",
                   DoubleValue (20.0),
                   MakeDoubleAccessor (&EhtErrorModelTest::m_snrEnd),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("SnrSteps", "Number of SNR steps.",
                   UintegerValue (20),
                   MakeUintegerAccessor (&EhtErrorModelTest::m_snrSteps),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

EhtErrorModelTest::EhtErrorModelTest ()
{
  m_numMcsValues = 12; // HT MCS values from 0 to 11
  m_fsrNist.resize (m_numMcsValues);
  m_fsrYans.resize (m_numMcsValues);
  m_fsrTable.resize (m_numMcsValues);

  for (uint32_t i = 0; i < m_numMcsValues; ++i)
    {
      m_fsrNist[i].resize (m_snrSteps);
      m_fsrYans[i].resize (m_snrSteps);
      m_fsrTable[i].resize (m_snrSteps);
    }
}

EhtErrorModelTest::~EhtErrorModelTest ()
{
}

void
EhtErrorModelTest::RunSimulation ()
{
  SetupSimulation ();
  Simulator::Stop (Seconds (1.0));
  Simulator::Run ();
  Simulator::Destroy ();
  CalculateFsr ();
  GeneratePlots ();
}

void
EhtErrorModelTest::SetupSimulation ()
{
  NodeContainer nodes;
  nodes.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211_EHT);

  NetDeviceContainer devices;
  devices = wifi.Install (phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice> (devices.Get (0));
  Ptr<WifiPhy> phy0 = device->GetPhy ();

  phy0->SetTxPowerStart (0.0);
  phy0->SetTxPowerEnd (0.0);
  phy0->SetRxGain (0.0);
  phy0->SetChannelWidth (20);

  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxError",
                   MakeCallback (&EhtErrorModelTest::SendPacket, this));
}

void
EhtErrorModelTest::SendPacket (Ptr<const Packet> packet, double snr, WifiTxVector txVector)
{
  uint32_t mcs = txVector.GetMode ().GetMcsValue ();
  if (mcs >= m_numMcsValues)
    {
      return;
    }

  for (uint32_t i = 0; i < m_snrSteps; ++i)
    {
      double currentSnr = m_snrStart + i * (m_snrEnd - m_snrStart) / m_snrSteps;
      if (snr >= currentSnr)
        {
          m_fsrNist[mcs][i] += 1.0;
          m_fsrYans[mcs][i] += 1.0;
          m_fsrTable[mcs][i] += 1.0;
        }
    }
}

void
EhtErrorModelTest::CalculateFsr ()
{
  for (uint32_t mcs = 0; mcs < m_numMcsValues; ++mcs)
    {
      for (uint32_t i = 0; i < m_snrSteps; ++i)
        {
          m_fsrNist[mcs][i] /= 100.0;
          m_fsrYans[mcs][i] /= 100.0;
          m_fsrTable[mcs][i] /= 100.0;
        }
    }
}

void
EhtErrorModelTest::GeneratePlots ()
{
  Gnuplot plot;
  plot.SetTitle ("Frame Success Rate vs SNR for NIST, YANS and Table-based Error Models");
  plot.SetTerminal ("png");
  plot.SetLegend ("SNR (dB)", "FSR");
  plot.SetExtra ("set grid");

  Gnuplot2dDataset nistDataset ("NIST");
  nistDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset yansDataset ("YANS");
  yansDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset tableDataset ("Table");
  tableDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  for (uint32_t i = 0; i < m_snrSteps; ++i)
    {
      double snr = m_snrStart + i * (m_snrEnd - m_snrStart) / m_snrSteps;
      for (uint32_t mcs = 0; mcs < m_numMcsValues; ++mcs)
        {
          nistDataset.Add (snr, m_fsrNist[mcs][i]);
          yansDataset.Add (snr, m_fsrYans[mcs][i]);
          tableDataset.Add (snr, m_fsrTable[mcs][i]);
        }
    }

  plot.AddDataset (nistDataset);
  plot.AddDataset (yansDataset);
  plot.AddDataset (tableDataset);

  std::ofstream plotFile ("fsr-vs-snr.plt");
  plot.GenerateOutput (plotFile);
  plotFile.close ();
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("EhtErrorModelComparison", LOG_LEVEL_INFO);

  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  EhtErrorModelTest test;
  test.RunSimulation ();

  return 0;
}