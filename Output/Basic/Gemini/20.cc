#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/qos-txop.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiCompressedBlockAck");

int main(int argc, char *argv[]) {
  bool enablePcap = true;
  uint32_t packetSize = 1024;
  std::string dataRate = "54Mbps";
  std::string phyMode = "OfdmRate54Mbps";
  uint32_t numPackets = 1000;

  CommandLine cmd;
  cmd.AddValue("pcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue("packetSize", "Size of packet", packetSize);
  cmd.AddValue("dataRate", "Data rate", dataRate);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue("numPackets", "Number of packets", numPackets);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer apNode, staNodes;
  apNode.Create(1);
  staNodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  QosWifiMacHelper wifiMac;
  wifiMac.SetType(WIFI_MAC_AP,
                  Mac48Address::GetBroadcast(),
                  Mac48Address::GetBroadcast());

  Ssid ssid = Ssid("ns-3-ssid");
  wifiMac.SetSSID(ssid);

  NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

  wifiMac.SetType(WIFI_MAC_QSTA,
                  Mac48Address::GetBroadcast(),
                  Mac48Address::GetBroadcast());
  NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);
  mobility.Install(staNodes);

  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t sinkPort = 50000;
  Address sinkAddress (InetSocketAddress (apInterfaces.GetAddress (0), sinkPort));

  UdpClientHelper client (sinkAddress);
  client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.001)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps = client.Install (staNodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = sink.Install (apNode.Get (0));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  Icmpv4EchoClientHelper echoClientHelper (apInterfaces.GetAddress (0), 1);
    echoClientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
    echoClientHelper.SetAttribute ("MaxPackets", UintegerValue (10));
    ApplicationContainer pingApps = echoClientHelper.Install (apNode.Get(0));
    pingApps.Start (Seconds (2.0));
    pingApps.Stop (Seconds (10.0));

  QosTxop::ConfigureBaAgreementParameters(staDevices.Get(0), apInterfaces.GetAddress (0), 0, 10, 100 * MicroSeconds(1));

  if (enablePcap) {
    wifiPhy.EnablePcap("wifi-compressed-blockack", apDevice.Get(0));
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}