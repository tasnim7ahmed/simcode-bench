#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocWifiSimulation");

class FixedRssPropagationLossModel : public PropagationLossModel {
public:
  static TypeId GetTypeId(void);
  FixedRssPropagationLossModel(double rss) : m_rss(rss) {}

protected:
  double DoCalcRxPower(double txPowerDbm, Ptr<MobilityModel> a, Ptr<MobilityModel> b) const override {
    return m_rss;
  }

private:
  double m_rss;
};

TypeId
FixedRssPropagationLossModel::GetTypeId(void)
{
  static TypeId tid = TypeId("FixedRssPropagationLossModel")
    .SetParent<PropagationLossModel>()
    .AddConstructor<FixedRssPropagationLossModel>();
  return tid;
}

int main(int argc, char *argv[])
{
  std::string phyMode("DsssRate1Mbps");
  double rss = -80.0; // dBm
  uint32_t packetSize = 1000;
  bool verbose = false;
  bool pcapTracing = false;
  double rxThreshold = -95.0; // dBm

  CommandLine cmd(__FILE__);
  cmd.AddValue("phyMode", "Wifi Phy mode (e.g., DsssRate1Mbps)", phyMode);
  cmd.AddValue("rss", "Received Signal Strength in dBm", rss);
  cmd.AddValue("packetSize", "Size of application packet sent", packetSize);
  cmd.AddValue("verbose", "Turn on all logging", verbose);
  cmd.AddValue("pcap", "Enable pcap tracing", pcapTracing);
  cmd.AddValue("rxThreshold", "Minimum RSS to receive packets", rxThreshold);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("AdHocWifiSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_ALL);
    LogComponentEnable("UdpServer", LOG_LEVEL_ALL);
  }

  NodeContainer nodes;
  nodes.Create(2);

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

  Ptr<FixedRssPropagationLossModel> lossModel = CreateObject<FixedRssPropagationLossModel>(rss);
  wifiChannel.AddPropagationLoss(lossModel);

  wifiPhy.SetChannel(wifiChannel.Create());
  wifiPhy.Set("TxPowerStart", DoubleValue(16.0));
  wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));
  wifiPhy.Set("RxGain", DoubleValue(0));
  wifiPhy.Set("RxSensitivity", DoubleValue(rxThreshold));

  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpServerHelper server(4000);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces.GetAddress(1), 4000);
  client.SetAttribute("MaxPackets", UintegerValue(1));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  if (pcapTracing) {
    wifiPhy.EnablePcapAll("adhoc-wifi-sim");
  }

  NS_LOG_INFO("Starting simulation...");
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}