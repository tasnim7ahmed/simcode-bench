#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  // Enable logging
  LogComponentEnable("TcpClient", LOG_LEVEL_INFO);
  LogComponentEnable("TcpServer", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer serverNode;
  serverNode.Create(1);
  NodeContainer clientNodes;
  clientNodes.Create(5);

  // Create point-to-point helper
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(serverNode);
  internet.Install(clientNodes);

  // Create and install network devices for server
  NetDeviceContainer serverDevices;
  for (int i = 0; i < 5; ++i) {
    NetDeviceContainer link = pointToPoint.Install(serverNode.Get(0), clientNodes.Get(i));
    serverDevices.Add(link.Get(0));
  }

  // Create and install network devices for clients
  NetDeviceContainer clientDevices;
  for (int i = 0; i < 5; ++i) {
    NetDeviceContainer link = pointToPoint.Install(serverNode.Get(0), clientNodes.Get(i));
    clientDevices.Add(link.Get(1));
  }
  
  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer serverInterfaces = address.Assign(serverDevices);
  Ipv4InterfaceContainer clientInterfaces;

  address.NewNetwork();
  address.SetBase("10.1.2.0", "255.255.255.0");
  for (int i = 0; i < 5; ++i) {
      NetDeviceContainer temp;
      temp.Add(clientDevices.Get(i));
      Ipv4InterfaceContainer clientInt = address.Assign(temp);
      clientInterfaces.Add(clientInt.Get(0));
      address.NewNetwork();
      address.SetBase(Ipv4Address(address.GetAddress(0)).GetNext().ConvertToInteger(), "255.255.255.0");
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create and install TCP server application
  uint16_t port = 50000;
  TcpServerHelper serverHelper(port);
  ApplicationContainer serverApp = serverHelper.Install(serverNode.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Create and install TCP client applications
  TcpClientHelper clientHelper(serverInterfaces.GetAddress(0), port);
  clientHelper.SetAttribute("MaxPackets", UintegerValue(10));
  clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (int i = 0; i < 5; ++i) {
    ApplicationContainer client = clientHelper.Install(clientNodes.Get(i));
    client.Start(Seconds(2.0 + i * 0.1));
    client.Stop(Seconds(9.0));
    clientApps.Add(client);
  }

  // Create FlowMonitor
  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  // Enable PCAP tracing
  pointToPoint.EnablePcapAll("star_tcp");

  // Set up animation
  AnimationInterface anim("star_tcp_animation.xml");
  anim.SetConstantPosition(serverNode.Get(0), 0.0, 0.0);
  for (int i = 0; i < 5; ++i) {
      anim.SetConstantPosition(clientNodes.Get(i), 5.0 * (i + 1), 5.0 * (i + 1));
  }

  // Run simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Analyze results
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
  }

  monitor->SerializeToXmlFile("star_tcp_flowmon.xml", true, true);

  Simulator::Destroy();
  return 0;
}