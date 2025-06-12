#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvAdHocSimulation");

class UdpTrafficHelper {
public:
  static void SetupUdpEcho(Ptr<Node> source, Ptr<Node> dest, uint16_t port, Time startTime) {
    UdpEchoClientHelper echoClient(dest->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(source);
    clientApps.Start(startTime);
    clientApps.Stop(Seconds(30.0));

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(dest);
    serverApps.Start(startTime);
    serverApps.Stop(Seconds(30.0));
  }
};

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  // Enable logging
  LogComponentEnable("AodvAdHocSimulation", LOG_LEVEL_INFO);
  
  // Create nodes
  NodeContainer nodes;
  nodes.Create(10);

  // Setup mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(100.0),
                                "DeltaY", DoubleValue(100.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 500, 0, 500)));
  mobility.Install(nodes);

  // Setup WiFi
  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"));

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Setup internet stack with AODV
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Setup UDP traffic
  uint16_t port = 9;
  for (uint32_t i = 0; i < 10; ++i) {
    uint32_t src = rand() % 10;
    uint32_t dst = rand() % 10;
    if (src == dst) continue;
    Time startTime = Seconds(1 + rand() % 10);
    UdpTrafficHelper::SetupUdpEcho(nodes.Get(src), nodes.Get(dst), port++, startTime);
  }

  // Enable pcap tracing
  wifiPhy.EnablePcapAll("aodv_adhoc_simulation");

  // Flow monitor setup
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(30.0));
  Simulator::Run();

  // Calculate metrics
  monitor->CheckForLostPackets();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  double totalRx = 0, totalTx = 0, totalDelay = 0;

  for (auto &flow : stats) {
    const auto &flowId = flow.first;
    const auto &flowStats = flow.second;
    totalTx += flowStats.txPackets;
    totalRx += flowStats.rxPackets;
    totalDelay += flowStats.delaySum.GetSeconds();
  }

  double pdr = (totalTx > 0) ? (totalRx / totalTx) : 0;
  double avgDelay = (totalRx > 0) ? (totalDelay / totalRx) : 0;

  NS_LOG_UNCOND("Packet Delivery Ratio: " << pdr);
  NS_LOG_UNCOND("Average End-to-End Delay: " << avgDelay << " s");

  Simulator::Destroy();
  return 0;
}