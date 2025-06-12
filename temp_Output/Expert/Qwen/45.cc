#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessInterferenceSimulation");

class InterferencePacketSocketServer : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("InterferencePacketSocketServer")
      .SetParent<Application>()
      .AddConstructor<InterferencePacketSocketServer>();
    return tid;
  }

  InterferencePacketSocketServer() {}
  virtual ~InterferencePacketSocketServer() {}

private:
  virtual void StartApplication(void) {}
  virtual void StopApplication(void) {}
};

int main(int argc, char *argv[])
{
  double primaryRss = -80.0; // dBm
  double interfererRss = -70.0; // dBm
  double timeOffset = 0.1; // seconds
  uint32_t packetSizePrimary = 1024;
  uint32_t packetSizeInterferer = 512;
  bool verbose = false;
  std::string phyMode("DsssRate1Mbps");
  std::string pcapFile = "wireless-interference.pcap";

  CommandLine cmd(__FILE__);
  cmd.AddValue("primaryRss", "Received signal strength of primary transmission in dBm", primaryRss);
  cmd.AddValue("interfererRss", "Received signal strength of interferer in dBm", interfererRss);
  cmd.AddValue("timeOffset", "Time offset between primary and interferer in seconds", timeOffset);
  cmd.AddValue("packetSizePrimary", "Size of packets sent by primary transmitter", packetSizePrimary);
  cmd.AddValue("packetSizeInterferer", "Size of packets sent by interferer", packetSizeInterferer);
  cmd.AddValue("verbose", "Turn on all Wifi logging", verbose);
  cmd.Parse(argc, argv);

  if (verbose)
  {
    LogComponentEnable("WifiMac", LOG_LEVEL_ALL);
    LogComponentEnable("YansWifiPhy", LOG_LEVEL_ALL);
    LogComponentEnable("InterferencePacketSocketServer", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(3); // 0: transmitter, 1: receiver, 2: interferer

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");

  // MAC layer configuration for all nodes
  NetDeviceContainer devices;
  mac.SetType("ns3::AdhocWifiMac");
  devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Set received signal strength for each node
  Ptr<YansWifiPhy> phy0 = DynamicCast<YansWifiPhy>(devices.Get(0)->GetPhy());
  Ptr<YansWifiPhy> phy1 = DynamicCast<YansWifiPhy>(devices.Get(1)->GetPhy());
  Ptr<YansWifiPhy> phy2 = DynamicCast<YansWifiPhy>(devices.Get(2)->GetPhy());

  // Create a PropagationLossModel to set fixed RSS
  Ptr<FixedRssLossModel> lossModel = CreateObject<FixedRssLossModel>();
  lossModel->SetRss(primaryRss);
  phy0->GetPhyLayerLossModel()->Add(lossModel);

  lossModel = CreateObject<FixedRssLossModel>();
  lossModel->SetRss(interfererRss);
  phy2->GetPhyLayerLossModel()->Add(lossModel);

  // Disable carrier sense for the interferer
  Ptr<WifiNetDevice> dev2 = DynamicCast<WifiNetDevice>(devices.Get(2));
  dev2->GetMac()->SetAttribute("CsmacaEnable", BooleanValue(false));

  // Install packet socket server on node 1 (receiver)
  PacketSocketHelper packetSocket;
  packetSocket.Install(nodes);

  // Setup applications
  PacketSocketAddress socketAddress;
  socketAddress.SetSingleDevice(devices.Get(1)->GetIfIndex());
  socketAddress.SetPhysicalAddress(devices.Get(1)->GetAddress());
  socketAddress.SetProtocol(0);

  // Primary transmitter application (node 0)
  OnOffHelper onoff("ns3::PacketSocketFactory", Address(socketAddress));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSizePrimary));

  ApplicationContainer apps;
  apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(1.0));

  // Interferer application (node 2)
  onoff.SetAttribute("PacketSize", UintegerValue(packetSizeInterferer));
  apps = onoff.Install(nodes.Get(2));
  apps.Start(Seconds(timeOffset));
  apps.Stop(Seconds(1.0));

  // Enable pcap tracing
  phy.EnablePcap(pcapFile, devices.Get(1));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}