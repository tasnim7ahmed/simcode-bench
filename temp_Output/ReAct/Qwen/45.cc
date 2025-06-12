#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessInterferenceSimulation");

class InterferenceHelper {
public:
  static void Setup(Ptr<WifiNetDevice> device, double rss, uint32_t packetSize, Time startTime, bool verbose) {
    Ptr<Packet> pkt = Create<Packet>(packetSize);
    Mac48Address dest = Mac48Address::GetBroadcast();
    if (verbose) {
      NS_LOG_INFO("Scheduling packet with RSS: " << rss << " dBm");
    }
    Simulator::Schedule(startTime, &WifiNetDevice::Send, device, pkt, dest, 0);
  }
};

int main(int argc, char *argv[]) {
  double primaryRss = -80.0; // dBm
  double interfererRss = -75.0; // dBm
  double timeOffset = 0.1; // seconds
  uint32_t primaryPacketSize = 1000;
  uint32_t interfererPacketSize = 1000;
  bool verbose = false;
  std::string pcapFile = "wireless_interference.pcap";

  CommandLine cmd(__FILE__);
  cmd.AddValue("primaryRss", "RSS of the primary transmitter in dBm", primaryRss);
  cmd.AddValue("interfererRss", "RSS of the interfering transmitter in dBm", interfererRss);
  cmd.AddValue("timeOffset", "Time offset between primary and interferer in seconds", timeOffset);
  cmd.AddValue("primaryPacketSize", "Packet size for primary transmission in bytes", primaryPacketSize);
  cmd.AddValue("interfererPacketSize", "Packet size for interference in bytes", interfererPacketSize);
  cmd.AddValue("verbose", "Enable verbose output", verbose);
  cmd.AddValue("pcapFile", "PCAP trace file name", pcapFile);
  cmd.Parse(argc, argv);

  Time stopTime = Seconds(1.0 + timeOffset + 0.1);

  NodeContainer nodes;
  nodes.Create(3); // 0: transmitter, 1: receiver, 2: interferer

  YansWifiPhyHelper phy;
  phy.Set("ChannelNumber", UintegerValue(1));
  phy.SetErrorRateModel("ns3::YansErrorRateModel");
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid("interference-network");
  mac.SetType("ns3::AdhocWifiMac",
              "Ssid", SsidValue(ssid),
              "ProbeRequestTimeout", TimeValue(MilliSeconds(0)),
              "MaxMissedBeacons", UintegerValue(0));

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Disable carrier sense for interferer
  Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Phy/State", StringValue("IDLE"));

  // Set RSS for transmitter and interferer
  Config::Set("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(primaryRss));
  Config::Set("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(primaryRss));
  Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(interfererRss));
  Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(interfererRss));

  if (verbose) {
    WifiHelper::EnableLogComponents();
    LogComponentEnable("WirelessInterferenceSimulation", LOG_LEVEL_INFO);
  }

  phy.EnablePcap(pcapFile, devices.Get(1), false);

  // Schedule transmissions
  Ptr<WifiNetDevice> txDevice = DynamicCast<WifiNetDevice>(devices.Get(0));
  InterferenceHelper::Setup(txDevice, primaryRss, primaryPacketSize, Seconds(0.1), verbose);

  Ptr<WifiNetDevice> intfDevice = DynamicCast<WifiNetDevice>(devices.Get(2));
  InterferenceHelper::Setup(intfDevice, interfererRss, interfererPacketSize, Seconds(0.1 + timeOffset), verbose);

  Simulator::Stop(stopTime);
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}