#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices2 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer devices3 = pointToPoint.Install(nodes.Get(0), nodes.Get(3));
  NetDeviceContainer devices4 = pointToPoint.Install(nodes.Get(0), nodes.Get(4));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);
  address.NewNetwork();
  Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);
  address.NewNetwork();
  Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);
  address.NewNetwork();
  Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient1(interfaces1.GetAddress(0), 9);
  echoClient1.SetAttribute("MaxPackets", UintegerValue(1000000));
  echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(1));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient2(interfaces2.GetAddress(0), 9);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(1000000));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(2));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient3(interfaces3.GetAddress(0), 9);
  echoClient3.SetAttribute("MaxPackets", UintegerValue(1000000));
  echoClient3.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps3 = echoClient3.Install(nodes.Get(3));
  clientApps3.Start(Seconds(2.0));
  clientApps3.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient4(interfaces4.GetAddress(0), 9);
  echoClient4.SetAttribute("MaxPackets", UintegerValue(1000000));
  echoClient4.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  echoClient4.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps4 = echoClient4.Install(nodes.Get(4));
  clientApps4.Start(Seconds(2.0));
  clientApps4.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum:  " << i->second.delaySum << "\n";
      std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  monitor->SerializeToXmlFile("star_topology.flowmon", true, true);

  pointToPoint.EnablePcapAll("star_topology");

  Simulator::Destroy();

  return 0;
}