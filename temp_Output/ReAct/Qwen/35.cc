#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ErrorModelComparison");

class ErrorModelComparison {
public:
  void RunSimulation(uint32_t frameSize, std::string errorModelName);
};

void
ErrorModelComparison::RunSimulation(uint32_t frameSize, std::string errorModelName)
{
  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;

  if (errorModelName == "Nist")
    {
      wifi.SetErrorRateModel("ns3::NistErrorRateModel");
    }
  else if (errorModelName == "Yans")
    {
      wifi.SetErrorRateModel("ns3::YansErrorRateModel");
    }
  else if (errorModelName == "Table")
    {
      wifi.SetErrorRateModel("ns3::TableBasedErrorRateModel");
    }

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("test")));
  NetDeviceContainer staDevice;
  staDevice = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("test")));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface;
  staInterface = address.Assign(staDevice);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevice);

  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(apInterface.GetAddress(0), port)));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(frameSize));

  ApplicationContainer clientApp = onoff.Install(wifiStaNode);
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(2.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer serverApp = sink.Install(wifiApNode);
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(3.0));

  Config::Connect("/NodeList/1/ApplicationList/0/Rx", MakeCallback([](std::string path, Ptr<const Packet> packet) {
    static uint32_t receivedPackets = 0;
    receivedPackets++;
    NS_LOG_DEBUG("Received packet: " << receivedPackets);
  }));

  Gnuplot2dDataset dataset;
  dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

  double snrStart = -5;
  double snrEnd = 30;
  double step = 2;
  uint32_t totalPackets = 1000;

  for (double snrDb = snrStart; snrDb <= snrEnd; snrDb += step)
    {
      phy.Set("TxPowerStart", DoubleValue(0.0));
      phy.Set("TxPowerEnd", DoubleValue(0.0));
      phy.Set("RxGain", DoubleValue(0.0));
      phy.Set("CcaMode1Threshold", DoubleValue(-79));
      phy.Set("EnergyDetectionThreshold", DoubleValue(-79 + 5));
      phy.Set("Snr", DoubleValue(snrDb));

      Simulator::Stop(Seconds(3.0));
      Simulator::Run();

      uint32_t rxCount = 0;
      for (uint32_t i = 0; i < serverApp.GetN(); ++i)
        {
          rxCount += DynamicCast<PacketSink>(serverApp.Get(i))->GetTotalRx();
        }

      double fsr = static_cast<double>(rxCount) / totalPackets;
      dataset.Add(snrDb, fsr);

      Simulator::Reset();
    }

  Gnuplot plot(errorModelName + "-OFDM-FSR-vs-SNR-" + std::to_string(frameSize) + "bytes.png");
  plot.SetTitle("Frame Success Rate vs SNR (" + errorModelName + ", Frame Size: " + std::to_string(frameSize) + " bytes)");
  plot.SetTerminal("png");
  plot.SetLegend("SNR (dB)", "Frame Success Rate");
  plot.AddDataset(dataset);

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(errorModelName + "-results.dat");
  plot.GenerateOutput(*stream);

  Simulator::Destroy();
}

int main(int argc, char *argv[])
{
  uint32_t frameSize = 1024;
  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "Size of application packet sent", frameSize);
  cmd.Parse(argc, argv);

  ErrorModelComparison comparison;

  comparison.RunSimulation(frameSize, "Nist");
  comparison.RunSimulation(frameSize, "Yans");
  comparison.RunSimulation(frameSize, "Table");

  return 0;
}