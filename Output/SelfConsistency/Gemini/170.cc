#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiLanSimulation");

int main(int argc, char *argv[]) {

  bool enablePcapTracing = true;
  std::string phyMode = "DsssRate11Mbps";

  CommandLine cmd;
  cmd.AddValue("enablePcap", "Enable pcap tracing", enablePcapTracing);
  cmd.Parse(argc, argv);

  // Configure Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  // Add a non-QoS MAC layer
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue(phyMode),
                               "ControlMode", StringValue(phyMode));

  // Setup the AP.
  Ssid ssid = Ssid("ns3-wifi");
  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));

  NodeContainer apNode;
  apNode.Create(1);

  NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

  // Setup the STA(s).
  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid));

  NodeContainer staNodes;
  staNodes.Create(2);

  NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

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

  mobility.Install(apNode);
  mobility.Install(staNodes);

  // Internet stack
  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(NetDeviceContainer::Create(apDevice, staDevices));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create server (STA 0)
  uint16_t port = 50000;
  UdpServerHelper server(port);
  server.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  server.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
  ApplicationContainer serverApps = server.Install(staNodes.Get(0));
  
  // Create client (STA 1)
  UdpClientHelper client(i.GetAddress(3), port);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(Time("0.001")));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  client.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
  client.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
  ApplicationContainer clientApps = client.Install(staNodes.Get(1));


  // Enable pcap tracing
  if (enablePcapTracing) {
    wifiPhy.EnablePcap("wifi-lan", apDevice);
    wifiPhy.EnablePcap("wifi-lan", staDevices);
  }


  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run the simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
  }

  Simulator::Destroy();

  return 0;
}