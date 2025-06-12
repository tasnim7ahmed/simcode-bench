#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line.h"

#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  std::string phyMode ("HeMcs0");
  double snrStart = 0.0;
  double snrEnd = 20.0;
  double snrStep = 1.0;
  uint32_t packetSize = 1000;
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("snrStart", "Start SNR (dB)", snrStart);
  cmd.AddValue ("snrEnd", "End SNR (dB)", snrEnd);
  cmd.AddValue ("snrStep", "SNR step (dB)", snrStep);
  cmd.AddValue ("packetSize", "Size of packet", packetSize);
  cmd.Parse (argc, argv);

  LogComponentEnable ("HeErrorRateModels", LOG_LEVEL_ERROR);

  std::vector<std::string> models = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};
  std::map<std::string, std::ofstream> dataFiles;
  std::map<std::string, Gnuplot2dDataset> datasets;
  std::map<std::string, std::string> titles = {
    {"NistErrorRateModel", "NIST"},
    {"YansErrorRateModel", "YANS"},
    {"TableBasedErrorRateModel", "Table Based"}
  };

  for (const auto& model : models) {
    std::string filename = phyMode + "_" + model + ".dat";
    dataFiles[model].open (filename.c_str());
    datasets[model].SetTitle (titles[model]);
    datasets[model].SetStyle (Gnuplot2dDataset::LINES_POINTS);
  }

  for (double snr = snrStart; snr <= snrEnd; snr += snrStep) {
    for (const auto& model : models) {
      Config::SetDefault ("ns3::WifiNetDevice::Phy/ErrorRateModel", StringValue (model));

      NodeContainer nodes;
      nodes.Create (2);

      WifiHelper wifi;
      wifi.SetStandard (WIFI_STANDARD_80211_HE);
      YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
      YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
      wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
      wifiChannel.AddPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
      wifiPhy.SetChannel (wifiChannel.Create ());

      WifiMacHelper wifiMac;
      Ssid ssid = Ssid ("ns-3-ssid");
      wifiMac.SetType ("ns3::StaWifiMac",
                      "Ssid", SsidValue (ssid),
                      "ActiveProbing", BooleanValue (false));

      NetDeviceContainer staDevices;
      staDevices = wifi.Install (wifiPhy, wifiMac, nodes.Get (0));

      wifiMac.SetType ("ns3::ApWifiMac",
                      "Ssid", SsidValue (ssid));

      NetDeviceContainer apDevices;
      apDevices = wifi.Install (wifiPhy, wifiMac, nodes.Get (1));

      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(20));

      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Frequency", UintegerValue(5180));

      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxGain", DoubleValue(10));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/RxGain", DoubleValue(10));

      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(10));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(10));

      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/HeSupported", BooleanValue(true));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardIntervalSupported", BooleanValue(true));

      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ControlRate", StringValue ("HeMcs0"));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/DataRate", StringValue (phyMode));

      Ptr<WifiNetDevice> apNetDevice = DynamicCast<WifiNetDevice> (apDevices.Get (0));
      Ptr<WifiNetDevice> staNetDevice = DynamicCast<WifiNetDevice> (staDevices.Get (0));
      Ptr<WifiPhy> apWifiPhy = apNetDevice->GetPhy ();
      Ptr<WifiPhy> staWifiPhy = staNetDevice->GetPhy ();

      double noiseFloor = -93;
      double txPower = 10;
      double rss = txPower - 40;
      double sinr = rss - (noiseFloor + snr);

      staWifiPhy->SetRxSensitivity (noiseFloor);

      Ipv4ListRoutingHelper ipv4RoutingHelper;
      Ipv4InterfaceContainer i = ipv4RoutingHelper.AssignAddresses (NetDeviceContainer (apDevices, staDevices));
      Ipv4AddressHelper ipv4;
      ipv4.SetBase ("10.1.1.0", "255.255.255.0");
      Ipv4InterfaceContainer i2 = ipv4.Assign (NetDeviceContainer (apDevices, staDevices));

      UdpClientHelper client (i2.GetAddress (1), 9);
      client.SetAttribute ("MaxPackets", UintegerValue (100));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.00001)));
      client.SetAttribute ("PacketSize", UintegerValue (packetSize));

      ApplicationContainer clientApps = client.Install (nodes.Get (0));
      clientApps.Start (Seconds (1.0));
      clientApps.Stop (Seconds (1.1));

      UdpServerHelper echoServer (9);
      ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
      serverApps.Start (Seconds (1.0));
      serverApps.Stop (Seconds (1.1));

      Simulator::Stop (Seconds (1.1));

      uint32_t totalPacketsSent = 0;
      uint32_t totalPacketsReceived = 0;

      Simulator::Run ();

      Ptr<Application> serverApp = serverApps.Get (0);
      Ptr<UdpServer> server = DynamicCast<UdpServer> (serverApp);
      totalPacketsReceived = server->GetReceived ();
      totalPacketsSent = 100;

      double successRate = (double)totalPacketsReceived / (double)totalPacketsSent;
      dataFiles[model] << snr << " " << successRate << std::endl;
      datasets[model].Add (snr, successRate);

      Simulator::Destroy ();
    }
  }

  Gnuplot gnuplot (phyMode + "_frame_success_rate.png");
  gnuplot.SetTitle ("Frame Success Rate vs SNR (" + phyMode + ")");
  gnuplot.SetLegend ("SNR (dB)", "Frame Success Rate");

  for (const auto& model : models) {
    dataFiles[model].close ();
    gnuplot.AddDataset (datasets[model]);
  }

  gnuplot.GenerateOutput (std::cout);

  return 0;
}