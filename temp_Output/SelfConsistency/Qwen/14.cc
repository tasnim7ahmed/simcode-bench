#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("QosWi-FiNetwork");

class TxopTrace {
public:
  static void TraceTxop(Ptr<OutputStreamWrapper> stream, uint8_t ac, Time duration) {
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << (uint32_t)ac << " " << duration.GetMicroSeconds() << std::endl;
  }
};

int main(int argc, char *argv[]) {
  uint32_t payloadSize = 1024;
  double distance = 1.0;
  double simulationTime = 10.0;
  bool enablePcap = false;
  bool enableRtsCts = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("payloadSize", "Payload size in bytes.", payloadSize);
  cmd.AddValue("distance", "Distance between nodes in meters.", distance);
  cmd.AddValue("simulationTime", "Simulation time in seconds.", simulationTime);
  cmd.AddValue("enablePcap", "Enable/disable pcap file generation.", enablePcap);
  cmd.AddValue("enableRtsCts", "Enable/disable RTS/CTS mechanism.", enableRtsCts);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", enableRtsCts ? StringValue("0") : StringValue("999999"));

  NodeContainer apNodes;
  NodeContainer staNodes;
  apNodes.Create(4);
  staNodes.Create(4);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

  WifiMacHelper mac;
  Ssid ssid;
  NetDeviceContainer apDevices;
  NetDeviceContainer staDevices;

  for (uint32_t i = 0; i < 4; ++i) {
    std::ostringstream oss;
    oss << "network-" << i;
    ssid = Ssid(oss.str());
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(Seconds(2.0)));
    apDevices.Add(wifi.Install(phy, mac, apNodes.Get(i)));

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevices.Add(wifi.Install(phy, mac, staNodes.Get(i)));
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(4),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);
  mobility.Install(staNodes);

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer apInterfaces;
  Ipv4InterfaceContainer staInterfaces;
  for (uint32_t i = 0; i < 4; ++i) {
    Ipv4InterfaceContainer interfaces = address.Assign(apDevices.Get(i));
    apInterfaces.Add(interfaces);
    interfaces = address.Assign(staDevices.Get(i));
    staInterfaces.Add(interfaces);
    address.NewNetwork();
  }

  UdpServerHelper serverHelper;
  ApplicationContainer serverApps;
  for (uint32_t i = 0; i < 4; ++i) {
    serverHelper = UdpServerHelper(staInterfaces.GetAddress(i, 0), 9);
    serverApps.Add(serverHelper.Install(apNodes.Get(i)));
  }

  OnOffHelper onoff("ns3::UdpSocketFactory", Address());
  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < 4; ++i) {
    Address destAddress(staInterfaces.GetAddress(i, 0));
    onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(destAddress, 9)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    clientApps.Add(onoff.Install(staNodes.Get(i)));
  }

  serverApps.Start(Seconds(0.0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  if (enablePcap) {
    phy.EnablePcapAll("qos_wifi_network");
  }

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> txopStream = asciiTraceHelper.CreateFileStream("txop_trace.txt");

  Config::Connect("/NodeList/*/DeviceList/*/Mac/TxopTrace", MakeBoundCallback(&TxopTrace::TraceTxop, txopStream));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowStats> stats = monitor->GetFlowStats();

  std::cout << "Throughput results:" << std::endl;
  for (std::map<FlowId, FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
    std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << "):" << std::endl;
    std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
    std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
    std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / simulationTime / 1000000 << " Mbps" << std::endl;
  }

  Simulator::Destroy();
  return 0;
}