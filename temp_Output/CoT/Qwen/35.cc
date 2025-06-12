#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OfdmErrorModelComparison");

class OfdmErrorModelComparison
{
public:
  void RunSimulation (uint32_t frameSize, std::string errorModelType);
};

void
OfdmErrorModelComparison::RunSimulation (uint32_t frameSize, std::string errorModelType)
{
  double snrMin = -5;
  double snrMax = 30;
  double snrStep = 1.0;
  uint32_t numPackets = 1000;

  Gnuplot plot ("ofdm_error_model_comparison_" + errorModelType + ".png");
  plot.SetTitle ("Frame Success Rate vs SNR for OFDM Modes");
  plot.SetTerminal ("png");
  plot.SetLegend ("SNR (dB)", "Frame Success Rate");

  Gnuplot2dDataset datasetDsssRate1Mbps;
  datasetDsssRate1Mbps.SetTitle ("OFDM-6Mbps");
  datasetDsssRate1Mbps.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset datasetDsssRate2Mbps;
  datasetDsssRate2Mbps.SetTitle ("OFDM-9Mbps");
  datasetDsssRate2Mbps.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset datasetDsssRate5_5Mbps;
  datasetDsssRate5_5Mbps.SetTitle ("OFDM-12Mbps");
  datasetDsssRate5_5Mbps.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset datasetDsssRate11Mbps;
  datasetDsssRate11Mbps.SetTitle ("OFDM-18Mbps");
  datasetDsssRate11Mbps.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211a);

  if (errorModelType == "Nist")
    {
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6Mbps"), "ControlMode", StringValue ("OfdmRate6Mbps"));
    }
  else if (errorModelType == "Yans")
    {
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6Mbps"), "ControlMode", StringValue ("OfdmRate6Mbps"));
      wifi.Set ("ErrorRateModel", TypeIdValue (YansErrorRateModel::GetTypeId ()));
    }
  else if (errorModelType == "Table")
    {
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6Mbps"), "ControlMode", StringValue ("OfdmRate6Mbps"));
      wifi.Set ("ErrorRateModel", TypeIdValue (NistErrorRateModel::GetTypeId ()));
    }

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  channel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NodeContainer nodes;
  nodes.Create (2);

  NetDeviceContainer devices;
  devices = wifi.Install (mac, channel.Create (), nodes);

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

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (frameSize));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Power", DoubleValue (10.0));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/SnrThresholdForFrameSuccess", DoubleValue (snrMin));

  for (double snr = snrMin; snr <= snrMax; snr += snrStep)
    {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/SnrThresholdForFrameSuccess", DoubleValue (snr));
      Simulator::Run ();

      double successCount = 0;
      for (uint32_t i = 0; i < clientApps.GetN (); ++i)
        {
          Ptr<UdpEchoClient> client = DynamicCast<UdpEchoClient> (clientApps.Get (i));
          successCount += client->GetSuccessfulPackets ();
        }

      double fsr = successCount / numPackets;

      if (wifi.GetDataMode () == "OfdmRate6Mbps")
        datasetDsssRate1Mbps.Add (snr, fsr);
      else if (wifi.GetDataMode () == "OfdmRate9Mbps")
        datasetDsssRate2Mbps.Add (snr, fsr);
      else if (wifi.GetDataMode () == "OfdmRate12Mbps")
        datasetDsssRate5_5Mbps.Add (snr, fsr);
      else if (wifi.GetDataMode () == "OfdmRate18Mbps")
        datasetDsssRate11Mbps.Add (snr, fsr);
    }

  plot.AddDataset (datasetDsssRate1Mbps);
  plot.AddDataset (datasetDsssRate2Mbps);
  plot.AddDataset (datasetDsssRate5_5Mbps);
  plot.AddDataset (datasetDsssRate11Mbps);

  std::ofstream plotFile ("ofdm_error_model_comparison_" + errorModelType + ".plt");
  plot.GenerateOutput (plotFile);
  plotFile.close ();

  Simulator::Destroy ();
}

int
main (int argc, char *argv[])
{
  uint32_t frameSize = 1024;
  std::string errorModelType = "Nist";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("frameSize", "Size of application packet sent", frameSize);
  cmd.AddValue ("errorModel", "Error rate model type: Nist, Yans, or Table", errorModelType);
  cmd.Parse (argc, argv);

  OfdmErrorModelComparison sim;
  sim.RunSimulation (frameSize, errorModelType);

  return 0;
}