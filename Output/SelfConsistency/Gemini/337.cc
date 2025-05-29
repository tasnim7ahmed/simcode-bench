#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpRandomWalk");

int main(int argc, char *argv[]) {
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application containers to log if set to true", verbose);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  // Disable fragmentation
  GlobalValue::Bind("Packet::EnableFragmentAggregation", BooleanValue(false));

  // Create nodes: STA (sender) and AP (receiver)
  NodeContainer staNodes;
  staNodes.Create(1);
  NodeContainer apNodes;
  apNodes.Create(1);

  // Setup WiFi PHY and channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

  // Configure the AP
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(Seconds(0.050)));

  NetDeviceContainer apDevices = wifi.Install(phy, mac, apNodes);

  // Configure the STA
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

  // Install mobility models
  MobilityHelper mobility;

  // AP is stationary
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);

  // STA moves randomly
  ObjectFactory pos;
  pos.SetTypeId("ns3::RandomRectanglePositionAllocator");
  pos.Set("X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  pos.Set("Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));

  Ptr<PositionAllocator> taPositionAlloc = pos.Create()->GetObject<PositionAllocator>();

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Mode", StringValue("Time"),
                            "Time", StringValue("2s"),
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                            "Bounds", StringValue("0|100|0|100"));

  mobility.Install(staNodes);

  // Install the internet stack
  InternetStackHelper stack;
  stack.Install(apNodes);
  stack.Install(staNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  // Create UDP client and server applications
  UdpServerHelper echoServer(9);

  ApplicationContainer serverApps = echoServer.Install(apNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper echoClient(apInterfaces.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Install FlowMonitor on all nodes
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Enable PCAP tracing
  phy.EnablePcap("wifi-udp-randomwalk", apDevices.Get(0));

  // Run the simulation
  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  // Print per flow statistics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
  }

  Simulator::Destroy();

  return 0;
}