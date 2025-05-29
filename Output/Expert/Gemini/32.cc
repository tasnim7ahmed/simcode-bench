#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/propagation-module.h"
#include "ns3/internet-module.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  uint32_t frameSize = 12000;
  double simulationTime = 1.0;
  std::string prefix = "he_mcs";
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("prefix", "Output file prefix", prefix);
  cmd.Parse(argc, argv);

  LogComponentEnable("UdpClient", LOG_LEVEL_ERROR);
  LogComponentEnable("UdpServer", LOG_LEVEL_ERROR);

  NodeContainer staNodes;
  staNodes.Create(1);
  NodeContainer apNodes;
  apNodes.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211_BE);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Create();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing",
              BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, staNodes);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconGeneration",
              BooleanValue(true));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, apNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX",
                                DoubleValue(0.0), "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0), "DeltaY",
                                DoubleValue(10.0), "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);
  mobility.Install(apNodes);

  InternetStackHelper internet;
  internet.Install(apNodes);
  internet.Install(staNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(staDevices);
  Ipv4InterfaceContainer iap = ipv4.Assign(apDevices);

  UdpServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(apNodes.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simulationTime + 1));

  UdpClientHelper echoClient(iap.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
  echoClient.SetAttribute("Interval", TimeValue(Time(MicroSeconds(1))));
  echoClient.SetAttribute("PacketSize", UintegerValue(frameSize));

  ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(simulationTime));

  Config::Set("/NodeList/*/$ns3::WifiNetDevice/Phy/ChannelWidth",
              UintegerValue(80));

  std::vector<WifiHeMcs> mcsValues = {
      WIFI_HE_MCS0,  WIFI_HE_MCS1,  WIFI_HE_MCS2,  WIFI_HE_MCS3,
      WIFI_HE_MCS4,  WIFI_HE_MCS5,  WIFI_HE_MCS6,  WIFI_HE_MCS7,
      WIFI_HE_MCS8,  WIFI_HE_MCS9,  WIFI_HE_MCS10, WIFI_HE_MCS11};

  std::vector<std::string> errorRateModelNames = {"NistErrorRateModel",
                                                    "YansErrorRateModel",
                                                    "TableBasedErrorRateModel"};

  for (WifiHeMcs mcs : mcsValues) {
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/HtConfiguration/HeMcs",
                EnumValue(mcs));

    for (const std::string &errorRateModelName : errorRateModelNames) {
      std::ofstream outputStream;
      std::string filename =
          prefix + "_" + errorRateModelName + "_" + std::to_string(mcs) + ".dat";
      outputStream.open(filename.c_str());

      for (double snr = 5; snr <= 30; snr += 1) {
        Config::Set(
            "/NodeList/*/$ns3::WifiNetDevice/Phy/ErrorModel/"
            "TypeId",
            TypeIdValue(TypeId::LookupByName(errorRateModelName)));
        if (errorRateModelName == "TableBasedErrorRateModel") {
          Config::Set(
              "/NodeList/*/$ns3::WifiNetDevice/Phy/ErrorModel/"
              "DataErrorModel",
              TypeIdValue(TypeId::LookupByName("MatrixBasedErrorRateModel")));
          Config::Set(
              "/NodeList/*/$ns3::WifiNetDevice/Phy/ErrorModel/"
              "ControlErrorModel",
              TypeIdValue(TypeId::LookupByName("MatrixBasedErrorRateModel")));
        }

        Config::Set(
            "/NodeList/*/$ns3::WifiNetDevice/Phy/Snr",
            DoubleValue(snr)); // set SNR

        Simulator::Run(Seconds(simulationTime));

        Ptr<UdpClient> client = DynamicCast<UdpClient>(clientApps.Get(0));
        uint32_t packetsSent = client->GetPacketsSent();
        uint32_t packetsReceived = client->GetPacketsReceived();

        double successRate = 0.0;
        if (packetsSent > 0) {
          successRate = (double)packetsReceived / packetsSent;
        }

        outputStream << snr << " " << successRate << std::endl;
        Simulator::Stop(Seconds(0.0));
        client->Reset();
      }

      outputStream.close();
    }
  }

  Simulator::Destroy();

  return 0;
}