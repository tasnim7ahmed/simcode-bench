#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EhtErrorRatePlot");

class EhtErrorRateExperiment
{
public:
  void Run();
};

void
EhtErrorRateExperiment::Run()
{
  uint32_t frameSize = 1472; // bytes
  double snrStart = 0;
  double snrStop = 30;
  double snrStep = 1;
  uint32_t totalPackets = 1000;

  std::vector<std::string> errorModels = {"ns3::NistWifiPhyThresholdsGenerator", "ns3::YansWifiPhyThresholdsGenerator", "ns3::TableBasedErrorRateModel"};
  std::vector<std::string> modelNames = {"NIST", "YANS", "TABLE"};

  Gnuplot gnuplot("eht-error-rate.png");
  gnuplot.SetTitle("Frame Success Rate vs SNR for different Error Rate Models");
  gnuplot.SetTerminal("png");
  gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");
  gnuplot.AppendExtra("set grid");

  for (size_t m = 0; m < errorModels.size(); ++m)
  {
    Gnuplot2dDataset dataset(modelNames[m]);
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue(errorModels[m]));

    for (double snr = snrStart; snr <= snrStop; snr += snrStep)
    {
      NodeContainer nodes;
      nodes.Create(2);

      YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
      YansWifiPhyHelper phy;
      phy.SetChannel(channel.Create());
      WifiMacHelper mac;
      WifiHelper wifi;
      wifi.SetStandard(WIFI_STANDARD_80211_EHT);
      wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                   "DataMode", StringValue("EhtMcs0"),
                                   "ControlMode", StringValue("EhtMcs0"));

      NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

      Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice>(devices.Get(0));
      Ptr<WifiPhy> phyTx = device->GetPhy();
      phyTx->SetAttribute("TxPowerStart", DoubleValue(10.0)); // dBm
      phyTx->SetAttribute("TxPowerEnd", DoubleValue(10.0));

      device = DynamicCast<WifiNetDevice>(devices.Get(1));
      Ptr<WifiPhy> phyRx = device->GetPhy();

      Packet::EnablePrinting();

      uint32_t packetSuccessCount = 0;

      for (uint32_t i = 0; i < totalPackets; ++i)
      {
        Ptr<Packet> p = Create<Packet>(frameSize);
        phyTx->Send(p, 0, WIFI_PREAMBLE_HE_SU, 800, WIFI_MOD_CLASS_EHT, HeMcsParams{0});
      }

      phyRx->TraceConnectWithoutContext("PhyRxDrop", MakeCallback(
          [&packetSuccessCount](Ptr<const Packet> p) { packetSuccessCount++; }));

      Simulator::Stop(Seconds(1.0));
      Simulator::Run();
      Simulator::Destroy();

      double fsr = static_cast<double>(totalPackets - packetSuccessCount) / totalPackets;
      dataset.Add(snr, fsr);
    }

    gnuplot.AddDataset(dataset);
  }

  std::ofstream plotFile("eht-error-rate.plt");
  gnuplot.GenerateOutput(plotFile);
  plotFile.close();
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("EhtErrorRatePlot", LOG_LEVEL_INFO);
  EhtErrorRateExperiment experiment;
  experiment.Run();
  return 0;
}