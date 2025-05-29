#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t nCsma = 4;

  NodeContainer csmaNodes;
  csmaNodes.Create(nCsma + 1); // +1 for the server node

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

  NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

  InternetStackHelper internet;
  internet.Install(csmaNodes);

  Ipv4AddressHelper ip;
  ip.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ipInterfaces = ip.Assign(csmaDevices);

  uint16_t port = 50000;
  Address sinkAddress(InetSocketAddress(ipInterfaces.GetAddress(nCsma), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install(csmaNodes.Get(nCsma)); //Server Node
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
  source.SetAttribute("MaxBytes", UintegerValue(0));

  ApplicationContainer sourceApps;
  for (uint32_t i = 0; i < nCsma; ++i) {
    sourceApps.Add(source.Install(csmaNodes.Get(i)));
  }

  sourceApps.Start(Seconds(2.0));
  sourceApps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("csma-animation.xml");
  anim.EnablePacketMetadata(true);
  anim.SetMaxPktsPerTraceFile(1000000);

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    std::cout << "  DelaySum: " << i->second.delaySum << "\n";
  }

  monitor->SerializeToXmlFile("csma-flowmon.xml", true, true);

  Simulator::Destroy();

  return 0;
}