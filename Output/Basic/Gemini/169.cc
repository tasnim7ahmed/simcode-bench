#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 50000;

  BulkSendHelper source("ns3::TcpSocketFactory",
                         InetSocketAddress(interfaces.GetAddress(1), port));
  source.SetAttribute("SendSize", UintegerValue(1448));
  source.SetAttribute("MaxBytes", UintegerValue(0));

  ApplicationContainer sourceApps = source.Install(nodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::TcpSocketFactory",
                       InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  pointToPoint.EnablePcapAll("p2p");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4GlobalRouting> globalRouting = Ipv4GlobalRouting::GetInstance();
  globalRouting->PrintRoutingTableAllAt(Seconds(10.0), std::cout);

  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("routing.routes", std::ios::out);
  globalRouting->SerializeRoutingTableAllAt(Seconds(10.0), *routingStream);

  Ptr<Ipv4> ipv4 = nodes.Get(0)->GetObject<Ipv4>();
  if (ipv4) {
      std::cout << "IPv4 address for Node 0: " << ipv4->GetAddress(1, 0).GetLocal() << std::endl;
  }
  ipv4 = nodes.Get(1)->GetObject<Ipv4>();
  if (ipv4) {
      std::cout << "IPv4 address for Node 1: " << ipv4->GetAddress(1, 0).GetLocal() << std::endl;
  }

  int j = 0;
  Time JitterSum;
  double Sum = 0;
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    std::cout << "  Flow " << i->first << " (" << i->second.srcAddress << " -> " << i->second.destAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}