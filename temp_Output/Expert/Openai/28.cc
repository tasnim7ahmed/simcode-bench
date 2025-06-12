#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

struct SimulationConfig {
  std::string phyStandard;
  bool erpProtection;
  bool shortPreamble;
  bool shortSlotTime;
  uint32_t payloadSize;
  std::string transportProto;
  std::string description;
};

double GetThroughput(Ptr<FlowMonitor> flowmon, FlowMonitorHelper& helper) {
  double throughput = 0;
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(helper.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();
  for (auto& flow : stats) {
    if (flow.second.rxPackets > 0) {
      throughput += (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1e6; // Mbps
    }
  }
  return throughput;
}

void RunScenario(const SimulationConfig& config) {
  uint32_t nB = 2; // 802.11b stations, non-HT
  uint32_t nG = 2; // 802.11g stations, non-HT
  uint32_t nHt = 2; // 802.11g HT stations

  NodeContainer wifiStaB;
  wifiStaB.Create(nB);
  NodeContainer wifiStaG;
  wifiStaG.Create(nG);
  NodeContainer wifiStaHt;
  wifiStaHt.Create(nHt);
  NodeContainer wifiAp;
  wifiAp.Create(1);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaB);
  mobility.Install(wifiStaG);
  mobility.Install(wifiStaHt);
  mobility.Install(wifiAp);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  WifiMacHelper mac;

  if (config.phyStandard == "802.11b") wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  else if (config.phyStandard == "802.11g") wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
  else wifi.SetStandard(WIFI_PHY_STANDARD_80211g); // Default

  phy.Set("ShortGuardEnabled", BooleanValue(config.shortSlotTime));
  phy.Set("ShortPreamble", BooleanValue(config.shortPreamble));
  phy.Set("ShortSlotTimeSupported", BooleanValue(config.shortSlotTime));

  Ssid ssid = Ssid("mixed-ssid");

  // b/g/HT non-HT station helpers
  NetDeviceContainer staDevB, staDevG, staDevHt, apDev;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
    "DataMode", StringValue("DsssRate11Mbps"),
    "ControlMode", StringValue("DsssRate11Mbps")
  );
  mac.SetType("ns3::StaWifiMac",
    "Ssid", SsidValue(ssid),
    "QosSupported", BooleanValue(false)
  );
  staDevB = wifi.Install(phy, mac, wifiStaB);

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
    "DataMode", StringValue("ErpOfdmRate54Mbps"),
    "ControlMode", StringValue("ErpOfdmRate24Mbps")
  );
  mac.SetType("ns3::StaWifiMac",
    "Ssid", SsidValue(ssid),
    "QosSupported", BooleanValue(false)
  );
  staDevG = wifi.Install(phy, mac, wifiStaG);

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
    "DataMode", StringValue("HtMcs7"),
    "ControlMode", StringValue("HtMcs0")
  );
  mac.SetType("ns3::StaWifiMac",
    "Ssid", SsidValue(ssid),
    "QosSupported", BooleanValue(true)
  );
  staDevHt = wifi.Install(phy, mac, wifiStaHt);

  mac.SetType("ns3::ApWifiMac",
    "Ssid", SsidValue(ssid),
    "ErpProtectionMode", StringValue(config.erpProtection ? "cts-to-self" : "none")
  );
  apDev = wifi.Install(phy, mac, wifiAp);

  InternetStackHelper stack;
  stack.Install(wifiStaB);
  stack.Install(wifiStaG);
  stack.Install(wifiStaHt);
  stack.Install(wifiAp);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staIfB, staIfG, staIfHt, apIfs;
  staIfB = address.Assign(staDevB);
  address.NewNetwork();
  staIfG = address.Assign(staDevG);
  address.NewNetwork();
  staIfHt = address.Assign(staDevHt);
  address.NewNetwork();
  apIfs = address.Assign(apDev);

  ApplicationContainer serverApps, clientApps;
  uint16_t port = 5000;
  double appStart = 1.0;
  double appStop = 11.0;

  // Set up payload-generating apps for all station types
  for (uint32_t i = 0; i < nB; ++i) {
    if (config.transportProto == "UDP") {
      UdpServerHelper server(port + i);
      serverApps.Add(server.Install(wifiAp.Get(0)));

      UdpClientHelper client(apIfs.GetAddress(0), port + i);
      client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
      client.SetAttribute("Interval", TimeValue(MicroSeconds(500)));
      client.SetAttribute("PacketSize", UintegerValue(config.payloadSize));
      clientApps.Add(client.Install(wifiStaB.Get(i)));
    } else {
      PacketSinkHelper sink("ns3::TcpSocketFactory",
                            InetSocketAddress(apIfs.GetAddress(0), port + i));
      serverApps.Add(sink.Install(wifiAp.Get(0)));

      OnOffHelper client("ns3::TcpSocketFactory",
                         InetSocketAddress(apIfs.GetAddress(0), port + i));
      client.SetAttribute("DataRate", DataRateValue(DataRate("20Mbps")));
      client.SetAttribute("PacketSize", UintegerValue(config.payloadSize));
      client.SetAttribute("StartTime", TimeValue(Seconds(appStart)));
      client.SetAttribute("StopTime", TimeValue(Seconds(appStop)));
      client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      clientApps.Add(client.Install(wifiStaB.Get(i)));
    }
  }
  for (uint32_t i = 0; i < nG; ++i) {
    if (config.transportProto == "UDP") {
      UdpServerHelper server(port + 10 + i);
      serverApps.Add(server.Install(wifiAp.Get(0)));

      UdpClientHelper client(apIfs.GetAddress(0), port + 10 + i);
      client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
      client.SetAttribute("Interval", TimeValue(MicroSeconds(500)));
      client.SetAttribute("PacketSize", UintegerValue(config.payloadSize));
      clientApps.Add(client.Install(wifiStaG.Get(i)));
    } else {
      PacketSinkHelper sink("ns3::TcpSocketFactory",
                            InetSocketAddress(apIfs.GetAddress(0), port + 10 + i));
      serverApps.Add(sink.Install(wifiAp.Get(0)));

      OnOffHelper client("ns3::TcpSocketFactory",
                         InetSocketAddress(apIfs.GetAddress(0), port + 10 + i));
      client.SetAttribute("DataRate", DataRateValue(DataRate("20Mbps")));
      client.SetAttribute("PacketSize", UintegerValue(config.payloadSize));
      client.SetAttribute("StartTime", TimeValue(Seconds(appStart)));
      client.SetAttribute("StopTime", TimeValue(Seconds(appStop)));
      client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      clientApps.Add(client.Install(wifiStaG.Get(i)));
    }
  }
  for (uint32_t i = 0; i < nHt; ++i) {
    if (config.transportProto == "UDP") {
      UdpServerHelper server(port + 20 + i);
      serverApps.Add(server.Install(wifiAp.Get(0)));

      UdpClientHelper client(apIfs.GetAddress(0), port + 20 + i);
      client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
      client.SetAttribute("Interval", TimeValue(MicroSeconds(500)));
      client.SetAttribute("PacketSize", UintegerValue(config.payloadSize));
      clientApps.Add(client.Install(wifiStaHt.Get(i)));
    } else {
      PacketSinkHelper sink("ns3::TcpSocketFactory",
                            InetSocketAddress(apIfs.GetAddress(0), port + 20 + i));
      serverApps.Add(sink.Install(wifiAp.Get(0)));

      OnOffHelper client("ns3::TcpSocketFactory",
                         InetSocketAddress(apIfs.GetAddress(0), port + 20 + i));
      client.SetAttribute("DataRate", DataRateValue(DataRate("20Mbps")));
      client.SetAttribute("PacketSize", UintegerValue(config.payloadSize));
      client.SetAttribute("StartTime", TimeValue(Seconds(appStart)));
      client.SetAttribute("StopTime", TimeValue(Seconds(appStop)));
      client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      clientApps.Add(client.Install(wifiStaHt.Get(i)));
    }
  }
  serverApps.Start(Seconds(appStart));
  serverApps.Stop(Seconds(appStop));
  clientApps.Start(Seconds(appStart));
  clientApps.Stop(Seconds(appStop));

  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

  Simulator::Stop(Seconds(appStop + 0.5));
  Simulator::Run();

  double totalThroughput = GetThroughput(flowmon, flowmonHelper);
  std::cout << config.description << " Total throughput: " << totalThroughput << " Mbps" << std::endl;

  Simulator::Destroy();
}

int main(int argc, char *argv[]) {
  std::vector<SimulationConfig> scenarios = {
    { "802.11g", true,  false, true,  512, "UDP", "802.11g, ERP Protection, Short Slot, UDP, 512B" },
    { "802.11g", false, true,  true,  1024,"UDP", "802.11g, No ERP, Short Preamble, Short Slot, UDP, 1024B" },
    { "802.11g", true,  true,  false, 512,  "TCP", "802.11g, ERP Protection, Short Preamble, TCP, 512B" },
    { "802.11g", false, false, true,  2048, "TCP", "802.11g, No ERP, Long Preamble, Short Slot, TCP, 2048B" }
  };

  for (auto& scenario : scenarios) {
    RunScenario(scenario);
  }
  return 0;
}