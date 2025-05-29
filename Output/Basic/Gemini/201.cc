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

  NodeContainer hubNode = NodeContainer(nodes.Get(0));
  NodeContainer spokeNodes;
  spokeNodes.Add(nodes.Get(1));
  spokeNodes.Add(nodes.Get(2));
  spokeNodes.Add(nodes.Get(3));
  spokeNodes.Add(nodes.Get(4));

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer hubDevices;
  NetDeviceContainer spokeDevices;

  for (int i = 0; i < 4; ++i) {
      NetDeviceContainer link = pointToPoint.Install(hubNode.Get(0), spokeNodes.Get(i));
      hubDevices.Add(link.Get(0));
      spokeDevices.Add(link.Get(1));
  }

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer hubInterfaces;
  Ipv4InterfaceContainer spokeInterfaces;
  for(int i = 0; i < 4; ++i){
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(hubDevices.Get(i), spokeDevices.Get(i)));
    hubInterfaces.Add(interfaces.Get(0));
    spokeInterfaces.Add(interfaces.Get(1));
    address.NewNetwork();
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(hubNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(hubInterfaces.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (int i = 0; i < 4; ++i) {
    clientApps.Add(echoClient.Install(spokeNodes.Get(i)));
  }
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(10.0));

  pointToPoint.EnablePcapAll("star");

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
      std::cout << "  Delay Sum:  " << i->second.delaySum << "\n";
      std::cout << "  Mean Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";
      std::cout << "  Packets Tx: " << i->second.txPackets << "\n";
      std::cout << "  Packets Rx: " << i->second.rxPackets << "\n";
      std::cout << "  Packet Loss Ratio: " << (double)(i->second.txPackets - i->second.rxPackets) / (double)i->second.txPackets << "\n";
    }

  monitor->SerializeToXmlFile("star.flowmon", true, true);

  Simulator::Destroy();
  return 0;
}