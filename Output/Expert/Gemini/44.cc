#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-application.h"
#include "ns3/command-line.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t packetSize = 1472;
  uint32_t numPackets = 100;
  double interval = 0.01;
  int64_t fixedRss = -80;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("packetSize", "Size of packets generated", packetSize);
  cmd.AddValue("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue("interval", "Interval between packets", interval);
  cmd.AddValue("fixedRss", "Fixed RSS value (dBm)", fixedRss);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.Set("RxGain", DoubleValue(0));
  wifiPhy.Set("TxGain", DoubleValue(0));

  FixedRssLossModel fixedRssLoss;
  fixedRssLoss.SetAttribute("Rss", IntegerValue(fixedRss));
  wifiPhy.Set("AntennaLossModel", StringValue("ns3::ConstantLossModel"));
  wifiPhy.Set("Loss", StringValue("ns3::FixedRssLossModel"));
  wifiPhy.AddPropagationLoss("ns3::FixedRssLossModel",
                              AttributeListValue(MakeAttributeValuePair("Rss", IntegerValue(fixedRss))));


  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel",
                                 AttributeListValue(MakeAttributeValuePair("Rss", IntegerValue(fixedRss))));


  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns-3-ssid");
  wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing",
                  BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));

  wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(1));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX",
                                DoubleValue(0.0), "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0), "DeltaY",
                                DoubleValue(10.0), "GridWidth", UintegerValue(3),
                                "Z", DoubleValue(0.0));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(staDevices);
  Ipv4InterfaceContainer j = ipv4.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  OnOffHelper onOffHelper("ns3::UdpSocketFactory",
                          Address(InetSocketAddress(j.GetAddress(0), port)));
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
  onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  ApplicationContainer clientApps = onOffHelper.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  wifiPhy.EnablePcap("wifi-simple-infra", apDevices.Get(0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}