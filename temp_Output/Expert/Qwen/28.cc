#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWifiNetwork");

class SimulationRunner {
public:
  SimulationRunner();
  void Run(void);
  void SetupNetwork(bool enableErpProtection, bool shortPpduFormat, bool shortSlotTime, uint32_t payloadSize, std::string transportProtocol);

private:
  NodeContainer wifiStaNodes;
  NodeContainer wifiApNode;
  NetDeviceContainer staDevices;
  NetDeviceContainer apDevice;
  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer apInterface;
  uint32_t m_payloadSize;
  std::string m_transportProtocol;
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor;

  void SetupPhyAndMac(WifiHelper& wifi, YansWifiPhyHelper& phy, WifiMacHelper& mac, bool enableErpProtection, bool shortPpduFormat, bool shortSlotTime);
  void SetupApplications();
  void ReportResults();
};

SimulationRunner::SimulationRunner() {
  wifiStaNodes.Create(5); // 5 stations: mix of b/g and HT
  wifiApNode.Create(1);
}

void SimulationRunner::Run(void) {
  std::vector<bool> erpProt = {false, true};
  std::vector<bool> shortPpdu = {false, true};
  std::vector<bool> shortSlot = {false, true};
  std::vector<uint32_t> payloads = {512, 1024, 1500};
  std::vector<std::string> protocols = {"UDP", "TCP"};

  for (auto erp : erpProt)
    for (auto ppdu : shortPpdu)
      for (auto slot : shortSlot)
        for (auto size : payloads)
          for (auto proto : protocols) {
            RngSeedManager::SetSeed(1);
            RngSeedManager::SetRun(1);
            m_payloadSize = size;
            m_transportProtocol = proto;
            SetupNetwork(erp, ppdu, slot, size, proto);
            Simulator::Stop(Seconds(10));
            Simulator::Run();
            ReportResults();
            Simulator::Destroy();
          }
}

void SimulationRunner::SetupNetwork(bool enableErpProtection, bool shortPpduFormat, bool shortSlotTime, uint32_t payloadSize, std::string transportProtocol) {
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

  WifiMacHelper mac;
  Ssid ssid = Ssid("mixed-wifi-network");
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "EnableBeaconJitter", BooleanValue(false));

  SetupPhyAndMac(wifi, phy, mac, enableErpProtection, shortPpduFormat, shortSlotTime);
  apDevice = wifi.Install(phy, mac, wifiApNode.Get(0));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  SetupPhyAndMac(wifi, phy, mac, enableErpProtection, shortPpduFormat, shortSlotTime);
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  apInterface = address.Assign(apDevice);
  staInterfaces = address.Assign(staDevices);

  SetupApplications();

  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelSettings",
              StringValue("{0, 20, BAND_2_4GHZ, 0}"));

  monitor = flowmon.InstallAll();
}

void SimulationRunner::SetupPhyAndMac(WifiHelper& wifi, YansWifiPhyHelper& phy, WifiMacHelper& mac,
                                     bool enableErpProtection, bool shortPpduFormat, bool shortSlotTime) {
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs7"),
                               "ControlMode", StringValue("HtMcs0"));

  phy.Set("ChannelWidth", UintegerValue(20));
  phy.Set("ShortPreambleEnabled", BooleanValue(shortPpduFormat));
  phy.Set("ShortSlotEnabled", BooleanValue(shortSlotTime));

  mac.Set("ErpProtectionMode", BooleanValue(enableErpProtection));
}

void SimulationRunner::SetupApplications() {
  uint16_t port = 9;

  if (m_transportProtocol == "UDP") {
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    client.SetAttribute("PacketSize", UintegerValue(m_payloadSize));

    ApplicationContainer clientApps = client.Install(wifiStaNodes);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));
  } else if (m_transportProtocol == "TCP") {
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    client.SetAttribute("PacketSize", UintegerValue(m_payloadSize));
    client.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer clientApp = client.Install(wifiStaNodes);
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));
  }
}

void SimulationRunner::ReportResults() {
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalThroughput = 0;
  uint32_t flowCount = 0;

  for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
    if (t.destinationPort == 9) {
      double duration = iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds();
      double throughput = (iter->second.rxBytes * 8.0) / duration / 1e6;
      totalThroughput += throughput;
      flowCount++;
    }
  }

  std::cout.precision(4);
  std::cout << "ERP Protection: " << (monitor->GetCounterIpv4().erpProtection ? "Yes" : "No")
            << ", Short PPDU: " << (monitor->GetCounterIpv4().shortPpdu ? "Yes" : "No")
            << ", Short Slot: " << (monitor->GetCounterIpv4().shortSlot ? "Yes" : "No")
            << ", Payload Size: " << m_payloadSize
            << ", Protocol: " << m_transportProtocol
            << ", Avg Throughput: " << (totalThroughput / std::max(1U, flowCount)) << " Mbps"
            << std::endl;
}

int main(int argc, char *argv[]) {
  LogComponentEnable("SimulationRunner", LOG_LEVEL_INFO);
  SimulationRunner runner;
  runner.Run();
  return 0;
}