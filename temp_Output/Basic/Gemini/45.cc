#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/on-off-application.h"
#include "ns3/udp-client.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInterference");

int main(int argc, char *argv[]) {
  bool verbose = false;
  std::string phyMode("DsssRate1Mbps");
  double primaryRs=-80;
  double interferingRs=-80;
  double timeOffset=0.000001;
  uint32_t packetSizePrimary=1000;
  uint32_t packetSizeInterfering=1000;
  bool pcapTracing = true;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("phyMode", "Wifi phy mode", phyMode);
  cmd.AddValue("primaryRs", "Received signal strength of primary signal (dBm)", primaryRs);
  cmd.AddValue("interferingRs", "Received signal strength of interfering signal (dBm)", interferingRs);
  cmd.AddValue("timeOffset", "Time offset between primary and interfering signals (seconds)", timeOffset);
  cmd.AddValue("packetSizePrimary", "Size of the primary packet", packetSizePrimary);
  cmd.AddValue("packetSizeInterfering", "Size of the interfering packet", packetSizeInterfering);
  cmd.AddValue("pcap", "Enable pcap tracing", pcapTracing);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("WifiInterference", LOG_LEVEL_ALL);
  }

  NodeContainer nodes;
  nodes.Create(3);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(primaryRs));
  Ptr<YansWifiChannel> channel = wifiChannel.Create();
  wifiPhy.SetChannel(channel);

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  uint16_t port = 9;

  OnOffHelper onoff("ns3::UdpSocketFactory",
                     Address(InetSocketAddress(interfaces.GetAddress(1), port)));
  onoff.SetConstantRate(DataRate("1Mb/s"), packetSizePrimary);

  ApplicationContainer app = onoff.Install(nodes.Get(0));
  app.Start(Seconds(1.0));
  app.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
  client.SetAttribute("PacketSize", UintegerValue(packetSizePrimary));
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  channel->SetPropagationLossModel("ns3::FixedRssLossModel", "Rss", DoubleValue(interferingRs));

  OnOffHelper onoffInterference("ns3::UdpSocketFactory",
                     Address(InetSocketAddress(interfaces.GetAddress(1), port)));
  onoffInterference.SetConstantRate(DataRate("1Mb/s"), packetSizeInterfering);

  ApplicationContainer appInterference = onoffInterference.Install(nodes.Get(2));
  appInterference.Start(Seconds(1.0+timeOffset));
  appInterference.Stop(Seconds(10.0+timeOffset));

  UdpClientHelper clientInterference(interfaces.GetAddress(1), port);
  clientInterference.SetAttribute("MaxPackets", UintegerValue(1000000));
  clientInterference.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
  clientInterference.SetAttribute("PacketSize", UintegerValue(packetSizeInterfering));
  ApplicationContainer clientAppsInterference = clientInterference.Install(nodes.Get(2));
  clientAppsInterference.Start(Seconds(1.0+timeOffset));
  clientAppsInterference.Stop(Seconds(10.0+timeOffset));

  if (pcapTracing) {
    wifiPhy.EnablePcap("wifi-interference", devices);
  }

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}