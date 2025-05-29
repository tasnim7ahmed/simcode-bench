#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/command-line.h"
#include "ns3/on-off-application.h"
#include "ns3/packet-sink.h"
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocFixedRss");

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t packetSize = 1000;
  std::string dataRate = "1Mbps";
  std::string phyRate = "DsssRate1Mbps";
  double fixedRss = -50; // dBm
  double rssThreshold = -90; // dBm
  uint32_t numPackets = 1;
  double interval = 1.0; // seconds
  bool pcapTracing = false;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if set.", verbose);
  cmd.AddValue("packetSize", "Size of packet sent in bytes", packetSize);
  cmd.AddValue("dataRate", "Data rate for OnOff application", dataRate);
  cmd.AddValue("phyRate", "Physical layer bitrate", phyRate);
  cmd.AddValue("fixedRss", "Fixed RSS (dBm)", fixedRss);
  cmd.AddValue("rssThreshold", "RSS threshold (dBm)", rssThreshold);
  cmd.AddValue("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue("interval", "Interval between packets (seconds)", interval);
  cmd.AddValue("pcap", "Enable PCAP tracing", pcapTracing);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("AdhocFixedRss", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  // Set the propagation loss model to FixedRssLossModel
  Ptr<FixedRssLossModel> fixedRssLossModel = CreateObject<FixedRssLossModel>();
  fixedRssLossModel->SetRss(fixedRss);
  wifiChannel.AddPropagationLoss(fixedRssLossModel);

  // Add a threshold
  Ptr<ThresholdReceptionModel> thresholdRx = CreateObject<ThresholdReceptionModel>();
  thresholdRx->SetThreshold(rssThreshold);
  wifiPhy.SetReceptionModel(thresholdRx);

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

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

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(devices);

  uint16_t port = 9; // Discard port (RFC 863)

  OnOffHelper onoff("ns3::UdpClient",
                     Address(InetSocketAddress(i.GetAddress(1), port)));
  onoff.SetConstantRate(DataRate(dataRate), packetSize);
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff.SetAttribute("MaxPackets", UintegerValue(numPackets));

  ApplicationContainer app = onoff.Install(nodes.Get(0));
  app.Start(Seconds(1.0));
  app.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
  app = sink.Install(nodes.Get(1));
  app.Start(Seconds(1.0));
  app.Stop(Seconds(10.0));

  if (pcapTracing) {
    wifiPhy.EnablePcapAll("adhoc-fixed-rss");
  }

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}