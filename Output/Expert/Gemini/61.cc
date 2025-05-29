#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper p2p_01;
  p2p_01.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p_01.SetChannelAttribute("Delay", StringValue("1ms"));
  p2p_01.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

  NetDeviceContainer devices_01 = p2p_01.Install(nodes.Get(0), nodes.Get(1));

  PointToPointHelper p2p_12;
  p2p_12.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  p2p_12.SetChannelAttribute("Delay", StringValue("10ms"));
  p2p_12.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

  NetDeviceContainer devices_12 = p2p_12.Install(nodes.Get(1), nodes.Get(2));

  PointToPointHelper p2p_23;
  p2p_23.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p_23.SetChannelAttribute("Delay", StringValue("1ms"));
  p2p_23.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

  NetDeviceContainer devices_23 = p2p_23.Install(nodes.Get(2), nodes.Get(3));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces_01 = address.Assign(devices_01);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces_12 = address.Assign(devices_12);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces_23 = address.Assign(devices_23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;
  BulkSendHelper source("ns3::TcpSocketFactory",
                         InetSocketAddress(interfaces_23.GetAddress(1), port));
  source.SetAttribute("SendSize", UintegerValue(1024));
  source.SetAttribute("MaxBytes", UintegerValue(0));

  ApplicationContainer sourceApps = source.Install(nodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(nodes.Get(3));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  Simulator::Schedule(Seconds(0.1), []() {
    std::ofstream config_file("results/config.txt");
    config_file << "Configuration:\n";
    config_file << "  Nodes: 4\n";
    config_file << "  Link 0-1: 10Mbps, 1ms\n";
    config_file << "  Link 1-2: 1Mbps, 10ms\n";
    config_file << "  Link 2-3: 10Mbps, 1ms\n";
    config_file << "  TCP Reno from n0 to n3\n";
    config_file.close();
  });

  AsciiTraceHelper ascii;
  p2p_01.EnablePcapAll("results/pcap/scratch_01", false);
  p2p_12.EnablePcapAll("results/pcap/scratch_12", false);
  p2p_23.EnablePcapAll("results/pcap/scratch_23", false);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  std::ofstream queueStatsFile("results/queueStats.txt");
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      queueStatsFile << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << "\n";
      queueStatsFile << "  Tx Packets: " << i->second.txPackets << "\n";
      queueStatsFile << "  Rx Packets: " << i->second.rxPackets << "\n";
      queueStatsFile << "  Lost Packets: " << i->second.lostPackets << "\n";
      queueStatsFile << "  Delay Sum: " << i->second.delaySum << "\n";
      queueStatsFile << "  Jitter Sum: " << i->second.jitterSum << "\n";
      queueStatsFile << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
  }
  queueStatsFile.close();

  Simulator::Destroy();

  return 0;
}