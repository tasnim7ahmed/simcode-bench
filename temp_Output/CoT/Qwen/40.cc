#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocWifiSimulation");

class FixedRssPropagationLossModel : public PropagationLossModel {
public:
  static TypeId GetTypeId(void);
  FixedRssPropagationLossModel(double rss) : m_rss(rss) {}

protected:
  double DoCalcRxPower(double txPowerW, Ptr<MobilityModel> a, Ptr<MobilityModel> b) const override {
    return m_rss;
  }

  int64_t DoAssignStreams(int64_t stream) override {
    return 0;
  }

private:
  double m_rss;
};

TypeId
FixedRssPropagationLossModel::GetTypeId(void) {
  static TypeId tid = TypeId("FixedRssPropagationLossModel")
    .SetParent<PropagationLossModel>()
    .AddConstructor<FixedRssPropagationLossModel>();
  return tid;
}

int main(int argc, char *argv[]) {
  double rss = -80; // dBm
  double rxThreshold = -95; // dBm
  uint32_t packetSize = 1000;
  bool verbose = true;
  bool pcap = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("rss", "Received Signal Strength (dBm)", rss);
  cmd.AddValue("rxThreshold", "Receive Threshold (dBm)", rxThreshold);
  cmd.AddValue("packetSize", "Size of packets to send", packetSize);
  cmd.AddValue("verbose", "Enable verbose logging", verbose);
  cmd.AddValue("pcap", "Enable pcap tracing", pcap);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("AdHocWifiSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ALL);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ALL);
  }

  NS_LOG_INFO("Creating nodes.");
  NodeContainer nodes;
  nodes.Create(2);

  NS_LOG_INFO("Setting up wifi.");
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();

  // Replace propagation loss model
  Ptr<FixedRssPropagationLossModel> lossModel = CreateObject<FixedRssPropagationLossModel>(rss);
  Ptr<PropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();

  channel.SetPropagationLossModel(lossModel);
  channel.SetPropagationDelayModel(delayModel);

  phy.SetChannel(channel.Create());
  phy.SetErrorRateModel(CreateObject<YansErrorRateModel>());
  phy.Set("TxPowerStart", DoubleValue(16.02)); // dBm
  phy.Set("TxPowerEnd", DoubleValue(16.02));
  phy.Set("RxGain", DoubleValue(0));
  phy.Set("RxSensitivity", DoubleValue(rxThreshold));

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  NS_LOG_INFO("Installing internet stack.");
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  NS_LOG_INFO("Setting up applications.");
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  if (pcap) {
    phy.EnablePcapAll("adhoc_wifi_simulation");
  }

  NS_LOG_INFO("Running simulation.");
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}