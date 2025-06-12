#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PointToPointTcpExample");

int main(int argc, char *argv[]) {
  // Enable logging
  LogComponentEnable("PointToPointTcpExample", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(3);

  // Configure Point-to-Point links
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Create NetDevices and Channels
  NetDeviceContainer d0d1 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer d1d2 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP Addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

  // Enable Global Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create a TCP server (sink) on node 2
  uint16_t port = 50000;
  Address sinkLocalAddress(InetSocketAddress(i1i2.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  // Create a TCP client (source) on node 0
  OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address(InetSocketAddress(i1i2.GetAddress(1), port)));
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("DataRate", StringValue("1Mbps"));
  ApplicationContainer clientApp = onOffHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  // Create FlowMonitor
  FlowMonitorHelper flowMonitorHelper;
  Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

  // Enable PCAP tracing
  pointToPoint.EnablePcapAll("p2p-tcp");

  // Run the simulation
  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  // Analyze the results
  flowMonitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
    NS_LOG_UNCOND("Tx Packets = " << i->second.txPackets);
    NS_LOG_UNCOND("Rx Packets = " << i->second.rxPackets);
    NS_LOG_UNCOND("Lost Packets = " << i->second.lostPackets);
    NS_LOG_UNCOND("Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps");
  }

  Simulator::Destroy();
  return 0;
}