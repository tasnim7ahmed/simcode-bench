#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OfdmErrorModelComparison");

class OfdmErrorModelTest
{
public:
  OfdmErrorModelTest();
  void RunSimulation(uint32_t frameSize);
  void GeneratePlots();

private:
  void SetupNodes();
  void SetupDevices();
  void SetupApplications();
  void SetErrorRateModel(const std::string& modelType);
  void CalculateFsrForSnrRange();
  double CalculateFrameSuccessRate(double snrDb, WifiMode mode);
  void AddDataSet(const std::string& modelType, WifiMode mode);

  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ptr<WifiNetDevice> m_wifiDevice;
  uint32_t m_frameSize;
  std::map<std::string, Gnuplot2dDataset> m_datasets;
};

OfdmErrorModelTest::OfdmErrorModelTest()
  : m_frameSize(1000)
{
  m_nodes.Create(2);
}

void OfdmErrorModelTest::RunSimulation(uint32_t frameSize)
{
  m_frameSize = frameSize;

  SetupNodes();
  SetupDevices();
  SetupApplications();

  // List of OFDM modes to test
  std::vector<WifiMode> ofdmModes = {
      WifiPhy::GetOfdmRate6Mbps(),
      WifiPhy::GetOfdmRate54Mbps(),
      WifiPhy::GetOfdmRate12Mbps(),
      WifiPhy::GetOfdmRate24Mbps()
  };

  for (const auto& mode : ofdmModes)
    {
      NS_LOG_UNCOND("Testing mode: " << mode.GetUniqueName());

      SetErrorRateModel("Nist");
      CalculateFsrForSnrRange();
      AddDataSet("Nist-" + mode.GetUniqueName(), mode);

      SetErrorRateModel("Yans");
      CalculateFsrForSnrRange();
      AddDataSet("Yans-" + mode.GetUniqueName(), mode);

      SetErrorRateModel("Table");
      CalculateFsrForSnrRange();
      AddDataSet("Table-" + mode.GetUniqueName(), mode);
    }
}

void OfdmErrorModelTest::GeneratePlots()
{
  Gnuplot plot = Gnuplot("ofdm_error_model_comparison.eps");
  plot.SetTitle("Frame Success Rate vs SNR for Different Error Models");
  plot.SetTerminal("postscript eps color enhanced");
  plot.SetLegend("SNR (dB)", "Frame Success Rate");
  plot.AppendExtra("set yrange [0:1]");
  plot.AppendExtra("set key outside");

  for (const auto& pair : m_datasets)
    {
      plot.AddDataset(pair.second);
    }

  std::ofstream plotFile("ofdm_error_model_comparison.plt");
  plot.GenerateOutput(plotFile);
  plotFile.close();
}

void OfdmErrorModelTest::SetupNodes()
{
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_nodes);

  InternetStackHelper internet;
  internet.Install(m_nodes);
}

void OfdmErrorModelTest::SetupDevices()
{
  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifi.SetChannel(channel.Create());

  m_devices = wifi.Install(wifiMac, m_nodes);
}

void OfdmErrorModelTest::SetupApplications()
{
  // Create a simple OnOff application that sends one packet and stops
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
  ApplicationContainer sinkApp = sink.Install(m_nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(1.0));

  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address("10.1.1.2"), 9));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(m_frameSize));

  ApplicationContainer apps = onoff.Install(m_nodes.Get(0));
  apps.Start(Seconds(0.1));
  apps.Stop(Seconds(1.0));
}

void OfdmErrorModelTest::SetErrorRateModel(const std::string& modelType)
{
  m_devices.Clear();
  m_wifiDevice = DynamicCast<WifiNetDevice>(m_devices.Get(0));
  if (!m_wifiDevice)
    {
      m_devices = NetDeviceContainer();
      SetupDevices();
      m_wifiDevice = DynamicCast<WifiNetDevice>(m_devices.Get(0));
    }

  Ptr<WifiPhy> phy = m_wifiDevice->GetPhy();
  if (modelType == "Nist")
    {
      phy->SetErrorRateModel(CreateObject<NistErrorRateModel>());
    }
  else if (modelType == "Yans")
    {
      phy->SetErrorRateModel(CreateObject<YansErrorRateModel>());
    }
  else if (modelType == "Table")
    {
      phy->SetErrorRateModel(CreateObject<TableBasedErrorRateModel>());
    }
  else
    {
      NS_FATAL_ERROR("Unknown error rate model: " << modelType);
    }
}

void OfdmErrorModelTest::CalculateFsrForSnrRange()
{
  double minSnrDb = -5;
  double maxSnrDb = 30;
  double step = 2;

  for (double snrDb = minSnrDb; snrDb <= maxSnrDb; snrDb += step)
    {
      Simulator::Stop(Seconds(0.01));
      Simulator::Run();

      // Force successful transmission at desired SNR by overriding actual received power
      Ptr<WifiPhy> phy = m_wifiDevice->GetPhy();
      phy->SetRxNoisePowerW(-80); // Fixed noise floor
      phy->SetTxPowerStart(phy->DbToPow(snrDb + (-80)) * 1e3); // Adjust TX power to get desired SNR

      Simulator::Stop(Seconds(0.02));
      Simulator::Run();

      bool success = false;
      for (auto iter = m_devices.Begin(); iter != m_devices.End(); ++iter)
        {
          Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice>(*iter);
          if (device)
            {
              device->SetReceiveCallback(MakeCallback(
                  [&](Ptr<NetDevice> dev, Ptr<const Packet> p, uint16_t protocol, const Address& addr) {
                    success = true;
                    return true;
                  }));
            }
        }

      Simulator::Stop(Seconds(0.03));
      Simulator::Run();

      double fpr = success ? 1.0 : 0.0;
      NS_LOG_DEBUG("SNR=" << snrDb << " FSR=" << fpr);
    }
}

double OfdmErrorModelTest::CalculateFrameSuccessRate(double snrDb, WifiMode mode)
{
  // This is a simplified approach - in reality you would use the error rate model directly
  // For demonstration purposes only
  Ptr<ErrorRateModel> errorModel = m_wifiDevice->GetPhy()->GetErrorRateModel();
  uint32_t payloadBits = m_frameSize * 8;
  double ber = errorModel->GetChunkSuccessRate(mode, snrDb, payloadBits / mode.GetSubcarriers());
  return std::pow(ber, payloadBits / mode.GetBitsPerTone());
}

void OfdmErrorModelTest::AddDataSet(const std::string& name, WifiMode mode)
{
  Gnuplot2dDataset dataset;
  dataset.SetTitle(name);
  dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

  for (double snrDb = -5; snrDb <= 30; snrDb += 2)
    {
      double fsr = CalculateFrameSuccessRate(snrDb, mode);
      dataset.Add(snrDb, fsr);
    }

  m_datasets[name] = dataset;
}

int main(int argc, char* argv[])
{
  uint32_t frameSize = 1000;

  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "Size of frames in bytes", frameSize);
  cmd.Parse(argc, argv);

  OfdmErrorModelTest tester;
  tester.RunSimulation(frameSize);
  tester.GeneratePlots();

  return 0;
}