#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer dev0_1 = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer dev1_2 = p2p.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iface0_1 = address.Assign(dev0_1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iface1_2 = address.Assign(dev1_2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 8080;
  Address bindAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  Address remoteAddress(InetSocketAddress(iface1_2.GetAddress(1), port));

  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", bindAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
  sinkApp.Start(Seconds(0.5));
  sinkApp.Stop(Seconds(10.0));

  OnOffHelper clientHelper("ns3::TcpSocketFactory", remoteAddress);
  clientHelper.SetAttribute("DataRate", StringValue("5Mbps"));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
  clientHelper.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  p2p.EnablePcapAll("p2p-tcp-3node");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.1));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

  double totalThroughput = 0.0;
  uint64_t totalLostPackets = 0;

  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for (auto const &flow : stats)
    {
      auto t = classifier->FindFlow(flow.first);
      if (t.destinationAddress == iface1_2.GetAddress(1))
        {
          double throughput = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1e6; // Mbps
          totalThroughput += throughput;
          totalLostPackets += flow.second.lostPackets;
        }
    }

  std::cout << "Total Throughput: " << totalThroughput << " Mbps" << std::endl;
  std::cout << "Total Lost Packets: " << totalLostPackets << std::endl;

  Simulator::Destroy();
  return 0;
}