#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/command-line.h"
#include "ns3/config-store.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeErrorRateValidation");

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t frameSize = 12000; // bytes
  std::string phyMode("He80MHz");
  double startSNR = 5.0;
  double stopSNR = 25.0;
  double stepSNR = 1.0;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue("phyMode", "802.11 PHY mode", phyMode);
  cmd.AddValue("startSNR", "Starting SNR value", startSNR);
  cmd.AddValue("stopSNR", "Stopping SNR value", stopSNR);
  cmd.AddValue("stepSNR", "SNR step", stepSNR);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("HeErrorRateValidation", LOG_LEVEL_INFO);
  }

  // Configure simulation parameters

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Configure channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());
  phy.Set("TxPowerStart", DoubleValue(16.0206));
  phy.Set("TxPowerEnd", DoubleValue(16.0206));

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211_HE);
  WifiMacHelper mac;
  Ssid ssid = Ssid("he-network");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(1));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconGeneration", BooleanValue(true),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(0));

  // Configure mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Configure Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
  interfaces = address.Assign(apDevices);

  // Configure UDP client application
  UdpClientHelper client(interfaces.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000000));
  client.SetAttribute("Interval", TimeValue(Time(0.00001))); //10us
  client.SetAttribute("PacketSize", UintegerValue(frameSize));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Iterate through MCS values and error rate models
  std::vector<std::string> errorRateModels = {"ns3::NistErrorRateModel", "ns3::YansErrorRateModel", "ns3::TableBasedErrorRateModel"};
  std::vector<HeMcs> mcsValues = {HE_MCS0, HE_MCS1, HE_MCS2, HE_MCS3, HE_MCS4, HE_MCS5, HE_MCS6, HE_MCS7,
                                   HE_MCS8, HE_MCS9, HE_MCS10, HE_MCS11};

  for (const std::string& errorRateModel : errorRateModels) {
    for (HeMcs mcs : mcsValues) {
      std::stringstream filename;
      filename << "he_" << phyMode << "_" << errorRateModel.substr(5, errorRateModel.size() - 5) << "_mcs" << static_cast<int>(mcs) << ".dat";
      std::ofstream output(filename.str());
      output << "# SNR FrameSuccessRate" << std::endl;

      for (double snr = startSNR; snr <= stopSNR; snr += stepSNR) {
        // Configure SNR
        Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(80));
        Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/HeSupported", BooleanValue(true));
        Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/ShortGuardIntervalSupported", BooleanValue(true));
        Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/GuardInterval", StringValue("ns3::HeGuardInterval800ns"));
        Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/ModulationClass", StringValue("He"));
        Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/ControlCodeRate", StringValue("NONE"));

        Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/TxGain", DoubleValue(snr));
        Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/RxNoiseFigure", DoubleValue(7)); //Default value

        Config::Set("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(80));
        Config::Set("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy/HeSupported", BooleanValue(true));
        Config::Set("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy/ShortGuardIntervalSupported", BooleanValue(true));
        Config::Set("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy/GuardInterval", StringValue("ns3::HeGuardInterval800ns"));
        Config::Set("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy/ModulationClass", StringValue("He"));
        Config::Set("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy/ControlCodeRate", StringValue("NONE"));

        Config::Set("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy/TxGain", DoubleValue(snr));
        Config::Set("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy/RxNoiseFigure", DoubleValue(7)); //Default value


        Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue(errorRateModel));
        Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/HeMcs", EnumValue(mcs));
        Config::Set("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy/HeMcs", EnumValue(mcs));

        // Run simulation
        Simulator::Stop(Seconds(10.0));
        Simulator::Run();

        // Calculate frame success rate
        uint64_t totalPacketsSent = DynamicCast<UdpClient>(clientApps.Get(0))->GetTotalPacketsSent();
        uint64_t totalPacketsReceived = DynamicCast<UdpServer>(serverApps.Get(0))->GetTotalPacketsReceived();
        double frameSuccessRate = (double)totalPacketsReceived / (double)totalPacketsSent;

        // Output results
        output << snr << " " << frameSuccessRate << std::endl;
        NS_LOG_INFO("SNR: " << snr << " Frame Success Rate: " << frameSuccessRate);
        Simulator::Destroy();
      }
      output.close();
    }
  }

  NS_LOG_INFO("Simulation finished");
  return 0;
}