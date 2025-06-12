#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/spectrum-module.h"
#include <fstream>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiPhyTest");

class PhyStats {
public:
  PhyStats(std::string filename);
  void LogSignalAndNoise(double signal, double noise, double snr);

private:
  std::ofstream m_logFile;
};

PhyStats::PhyStats(std::string filename) {
  m_logFile.open(filename);
  m_logFile << "Time(s),Signal(dBm),Noise(dBm),SNR(dB)" << std::endl;
}

void PhyStats::LogSignalAndNoise(double signal, double noise, double snr) {
  m_logFile << Simulator::Now().GetSeconds() << "," << signal << "," << noise << "," << snr << std::endl;
}

int main(int argc, char *argv[]) {
  uint32_t payloadSize = 1472;       // bytes
  double simulationTime = 10.0;      // seconds
  double distance = 1.0;             // meters
  bool enablePcap = false;
  bool useUdp = true;
  std::string phyType = "Yans";
  uint8_t mcsIndex = 4;
  uint16_t channelWidth = 20;
  bool shortGuardInterval = false;
  std::string outputFilename = "phy_stats.csv";

  CommandLine cmd(__FILE__);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("distance", "Distance between nodes in meters", distance);
  cmd.AddValue("enablePcap", "Enable PCAP capture", enablePcap);
  cmd.AddValue("useUdp", "Use UDP if true, else TCP", useUdp);
  cmd.AddValue("phyType", "PHY model type: Yans or Spectrum", phyType);
  cmd.AddValue("mcsIndex", "MCS index (0-7 for HT)", mcsIndex);
  cmd.AddValue("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
  cmd.AddValue("shortGuardInterval", "Enable short guard interval", shortGuardInterval);
  cmd.AddValue("outputFilename", "Output CSV file name", outputFilename);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);

  if (phyType == "Spectrum") {
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs" + std::to_string(mcsIndex)),
                                 "ControlMode", StringValue("HtMcs0"));
    Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
    Ptr<FriisSpectrumPropagationLossModel> lossModel = CreateObject<FriisSpectrumPropagationLossModel>();
    spectrumChannel->AddPropagationLossModel(lossModel);
    Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
    spectrumChannel->SetPropagationDelayModel(delayModel);
    wifi.SetChannel(spectrumChannel);
  } else {
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(20));
    phy.Set("TxPowerEnd", DoubleValue(20));
    phy.Set("TxGain", DoubleValue(0));
    phy.Set("RxGain", DoubleValue(0));
    wifi.SetPhy(phy);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs" + std::to_string(mcsIndex)),
                                 "ControlMode", StringValue("HtMcs0"));
  }

  wifi.SetChannelWidth(channelWidth);
  Config::SetDefault("ns3::WifiNetDevice::ShortGuardIntervalSupported", BooleanValue(shortGuardInterval));

  Ssid ssid = Ssid("ns3-wifi-phy-test");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice = wifi.Install(mac, wifiStaNode);
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  Address sinkAddress;
  if (useUdp) {
    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(wifiStaNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    sinkAddress = InetSocketAddress(staInterface.GetAddress(0), 9);
    UdpClientHelper client(apInterface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));
  } else {
    uint16_t port = 50000;
    Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", localAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(wifiStaNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    sinkAddress = InetSocketAddress(staInterface.GetAddress(0), port);
    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer clientApp = clientHelper.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));
  }

  if (enablePcap) {
    wifiPhyHelper.EnablePcap("wifi_phy_test_ap", apDevice.Get(0));
    wifiPhyHelper.EnablePcap("wifi_phy_test_sta", staDevice.Get(0));
  }

  PhyStats stats(outputFilename);

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxOk", MakeCallback(&PhyStats::LogSignalAndNoise, &stats));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}