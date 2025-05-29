#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 8080;

  // Server
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  // Client
  OnOffHelper onOffHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = onOffHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  // Flow Monitor
  FlowMonitorHelper flowMonitorHelper;
  Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  flowMonitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

  for (auto it = stats.begin(); it != stats.end(); ++it) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
    std::cout << "Flow " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << it->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << it->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << it->second.lostPackets << "\n";
    std::cout << "  Throughput: " << it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.Seconds() - it->second.timeFirstTxPacket.Seconds()) / 1000000 << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}