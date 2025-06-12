#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeErrorRateValidation");

class HeErrorRateExperiment
{
public:
  HeErrorRateExperiment();
  void Run(uint32_t frameSize, std::string rateModel);
  Gnuplot2dDataset GetDataset() const;
private:
  Gnuplot2dDataset m_dataset;
};

HeErrorRateExperiment::HeErrorRateExperiment()
{
}

void
HeErrorRateExperiment::Run(uint32_t frameSize, std::string rateModel)
{
  NodeContainer nodes;
  nodes.Create(2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211_ax_5GHZ);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("999999"));
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("999999"));

  if (rateModel == "YANS")
    {
      wifi.SetRemoteStationManager("ns3::YansWifiManager");
    }
  else if (rateModel == "NIST")
    {
      wifi.SetRemoteStationManager("ns3::NistWifiManager");
    }
  else if (rateModel == "Table")
    {
      wifi.SetRemoteStationManager("ns3::TableBasedWifiManager");
    }

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;
  UdpEchoServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(1.0));

  UdpEchoClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(1));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(frameSize));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(0.0));
  clientApps.Stop(Seconds(1.0));

  phy.EnablePcap("HeErrorRateValidation", devices);

  double snrStart = 0.0;
  double snrEnd = 30.0;
  uint32_t snrSteps = 16;

  for (uint32_t i = 0; i < snrSteps; ++i)
    {
      double snr = snrStart + (snrEnd - snrStart) * i / (snrSteps - 1);
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelSettings/Snr", DoubleValue(snr));

      Simulator::Stop(Seconds(1.0));
      Simulator::Run();

      uint32_t received = 0;
      uint32_t sent = 1;

      for (uint32_t j = 0; j < serverApps.GetN(); ++j)
        {
          UdpEchoServer* serverApp = dynamic_cast<UdpEchoServer*>(PeekPointer(serverApps.Get(j)));
          if (serverApp)
            {
              received += serverApp->GetReceived();
            }
        }

      double fsr = static_cast<double>(received) / sent;
      m_dataset.Add(Simulator::Now().GetSeconds(), fsr);
    }

  Simulator::Destroy();
}

Gnuplot2dDataset
HeErrorRateExperiment::GetDataset() const
{
  return m_dataset;
}

int main(int argc, char *argv[])
{
  uint32_t frameSize = 1000;
  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "The size of the transmitted frames in bytes", frameSize);
  cmd.Parse(argc, argv);

  GnuplotCollection plotCollection("he-error-rate-validation.plt");
  plotCollection.SetTitle("Frame Success Rate vs SNR for HE Rates");
  plotCollection.SetTerminal("png");

  Gnuplot plot;
  plot.SetTitle("Frame Success Rate vs SNR");
  plot.Set xlabel("SNR (dB)");
  plot.Set ylabel("Frame Success Rate");

  std::vector<std::string> models = {"YANS", "NIST", "Table"};
  for (auto model : models)
    {
      HeErrorRateExperiment experiment;
      experiment.Run(frameSize, model);
      Gnuplot2dDataset dataset = experiment.GetDataset();
      dataset.SetTitle(model + " Model");
      dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
      plot.AddDataset(dataset);
    }

  plotCollection.AddPlot(plot);
  plotCollection.GenerateOutput(std::cout);

  return 0;
}