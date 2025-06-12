#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

void
PrintFlowStats(Ptr<FlowMonitor> flowMonitor, FlowMonitorHelper& flowHelper)
{
  flowMonitor->CheckForLostPackets();
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
  for (auto const& flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = flowHelper.GetClassifier()->FindFlow(flow.first);
      std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
      std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
      Time dur = flow.second.timeLastRxPacket - flow.second.timeFirstTxPacket;
      double th = flow.second.rxBytes * 8.0 / dur.GetSeconds() / 1e6;
      std::cout << "  Throughput: " << th << " Mbps\n";
    }
}

void
RunSimulation(bool enableRtsCts, std::string runLabel)
{
  // Create 3 nodes
  NodeContainer nodes;
  nodes.Create(3);

  // Mobility: set up nodes at fixed locations (node 0 and node 2 are "hidden" from each other)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
  posAlloc->Add(Vector(0.0, 0.0, 0.0));   // Node 0
  posAlloc->Add(Vector(100.0, 0.0, 0.0)); // Node 1
  posAlloc->Add(Vector(200.0, 0.0, 0.0)); // Node 2
  mobility.SetPositionAllocator(posAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Wi-Fi Channel & PHY configuration
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  Ptr<MatrixPropagationLossModel> matrix = CreateObject<MatrixPropagationLossModel>();
  matrix->SetDefaultLoss(200); // large value for non-adjacent (node 0 <-> node 2)
  matrix->SetLoss(nodes.Get(0)->GetObject<MobilityModel>(), nodes.Get(1)->GetObject<MobilityModel>(), 50);
  matrix->SetLoss(nodes.Get(1)->GetObject<MobilityModel>(), nodes.Get(0)->GetObject<MobilityModel>(), 50);
  matrix->SetLoss(nodes.Get(1)->GetObject<MobilityModel>(), nodes.Get(2)->GetObject<MobilityModel>(), 50);
  matrix->SetLoss(nodes.Get(2)->GetObject<MobilityModel>(), nodes.Get(1)->GetObject<MobilityModel>(), 50);
  wifiChannel.SetPropagationLossModel(matrix);
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());
  wifiPhy.Set("RxGain", DoubleValue(0));

  // Wi-Fi MAC and DCF configuration: ad-hoc 802.11b
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  // RTS/CTS configuration
  if (enableRtsCts)
    {
      Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(100));
    }
  else
    {
      Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(2200)); // disable
    }
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                              "DataMode", StringValue("DsssRate11Mbps"),
                              "ControlMode", StringValue("DsssRate1Mbps"));

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Internet stack & IP addressing
  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces = ipv4.Assign(devices);

  // OnOff applications (node 0 -> node 1, node 2 -> node 1), CBR
  uint16_t port1 = 9001, port2 = 9002;
  ApplicationContainer apps;

  // CBR params
  double sendRate = 2e6;               // 2 Mbps
  uint32_t packetSize = 512;           // 512 bytes

  // Sink on node 1 for flow from node 0
  PacketSinkHelper sinkHelper1("ns3::UdpSocketFactory",
                              InetSocketAddress(ifaces.GetAddress(1), port1));
  ApplicationContainer sinkApp1 = sinkHelper1.Install(nodes.Get(1));
  sinkApp1.Start(Seconds(0.0));
  sinkApp1.Stop(Seconds(10.0));

  // Sink on node 1 for flow from node 2
  PacketSinkHelper sinkHelper2("ns3::UdpSocketFactory",
                              InetSocketAddress(ifaces.GetAddress(1), port2));
  ApplicationContainer sinkApp2 = sinkHelper2.Install(nodes.Get(1));
  sinkApp2.Start(Seconds(0.0));
  sinkApp2.Stop(Seconds(10.0));

  // OnOff from node 0 to node 1
  OnOffHelper onoff0("ns3::UdpSocketFactory",
                    InetSocketAddress(ifaces.GetAddress(1), port1));
  onoff0.SetAttribute("DataRate", DataRateValue(DataRate(sendRate)));
  onoff0.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff0.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  onoff0.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
  onoff0.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff0.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer app0 = onoff0.Install(nodes.Get(0));

  // OnOff from node 2 to node 1 (slightly staggered)
  OnOffHelper onoff2("ns3::UdpSocketFactory",
                    InetSocketAddress(ifaces.GetAddress(1), port2));
  onoff2.SetAttribute("DataRate", DataRateValue(DataRate(sendRate)));
  onoff2.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff2.SetAttribute("StartTime", TimeValue(Seconds(1.02)));
  onoff2.SetAttribute("StopTime", TimeValue(Seconds(9.02)));
  onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer app2 = onoff2.Install(nodes.Get(2));

  // Flow monitor
  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  std::cout << "\n---------------------\n";
  std::cout << "Simulation with RTS/CTS ";
  if (enableRtsCts)
    {
      std::cout << "enabled (threshold 100 bytes)";
    }
  else
    {
      std::cout << "disabled";
    }
  std::cout << ": " << runLabel << "\n";
  PrintFlowStats(monitor, flowHelper);

  Simulator::Destroy();
}

int
main(int argc, char *argv[])
{
  std::cout << "==== Classical Hidden Terminal Simulation ====\n";
  RunSimulation(false, "No RTS/CTS");
  RunSimulation(true, "With RTS/CTS");
  return 0;
}