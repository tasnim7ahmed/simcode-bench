#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool enableErpProtection = false;
  bool useShortPreamble = false;
  bool useShortSlotTime = false;
  uint32_t payloadSize = 1472;
  std::string transportProtocol = "Udp";

  CommandLine cmd;
  cmd.AddValue("enableErpProtection", "Enable ERP protection (default: false)", enableErpProtection);
  cmd.AddValue("useShortPreamble", "Use short preamble (default: false)", useShortPreamble);
  cmd.AddValue("useShortSlotTime", "Use short slot time (default: false)", useShortSlotTime);
  cmd.AddValue("payloadSize", "Payload size in bytes (default: 1472)", payloadSize);
  cmd.AddValue("transportProtocol", "Transport protocol (Udp or Tcp) (default: Udp)", transportProtocol);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer staNodes;
  staNodes.Create(6);

  NodeContainer apNode;
  apNode.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");

  // Setup b/g station
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevicesBg = wifi.Install(phy, mac, staNodes.Get(0));
  NetDeviceContainer staDevicesBg1 = wifi.Install(phy, mac, staNodes.Get(1));

  // Setup HT/non-HT station
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevicesHt = wifi.Install(phy, mac, staNodes.Get(2));
  NetDeviceContainer staDevicesHt1 = wifi.Install(phy, mac, staNodes.Get(3));
  NetDeviceContainer staDevicesHt2 = wifi.Install(phy, mac, staNodes.Get(4));
  NetDeviceContainer staDevicesHt3 = wifi.Install(phy, mac, staNodes.Get(5));

  // Setup AP
  wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconGeneration", BooleanValue(true),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  //Configure Wifi Channel
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(20));

  if (enableErpProtection) {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/ErpProtection", BooleanValue(true));
  }

  if (useShortPreamble) {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortPreambleEnabled", BooleanValue(true));
  }

  if (useShortSlotTime) {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Slot", TimeValue(MicroSeconds(9)));
  }

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);

  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("15.0"),
                                "Y", StringValue("0.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=5.0]"));
  mobility.Install(apNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(apNode);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces;
  for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
    NetDeviceContainer staDevice;
    staDevice.Add(staDevicesBg.Get(i/2));
    staDevice.Add(staDevicesBg1.Get(i/2));
    staDevice.Add(staDevicesHt.Get(i/4));
    staDevice.Add(staDevicesHt1.Get(i/4));
    staDevice.Add(staDevicesHt2.Get(i/4));
    staDevice.Add(staDevicesHt3.Get(i/4));
    if (i < 2){
      staInterfaces.Add(address.Assign(staDevicesBg.Get(i)));
    }
    else{
      staInterfaces.Add(address.Assign(wifi.GetWifiNetDevice(staNodes.Get(i),0)));
    }
  }

  // Applications
  uint16_t port = 9;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper sinkHelper(transportProtocol == "Udp" ? "ns3::UdpSocketFactory" : "ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(apNode.Get(0));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  OnOffHelper onOffHelper(transportProtocol == "Udp" ? "ns3::UdpSocketFactory" : "ns3::TcpSocketFactory", Address());
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));
  onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("54Mbps")));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
    AddressValue remoteAddress(InetSocketAddress(apInterfaces.GetAddress(0), port));
    onOffHelper.SetAttribute("Remote", remoteAddress);
    clientApps.Add(onOffHelper.Install(staNodes.Get(i)));
  }

  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}