#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarNetworkSimulation");

int main(int argc, char* argv[]) {
  // Enable logging
  LogComponent::Enable("StarNetworkSimulation", LOG_LEVEL_INFO);

  // Set simulation time and data rate
  double simulationTime = 10.0;
  std::string dataRate = "5Mbps";
  std::string delay = "2ms";

  // Allow command-line arguments to override simulation parameters
  CommandLine cmd;
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("dataRate", "Data rate of the links", dataRate);
  cmd.AddValue("delay", "Delay of the links", delay);
  cmd.Parse(argc, argv);

  // Create nodes: one server and five clients
  NodeContainer serverNode;
  serverNode.Create(1);

  NodeContainer clientNodes;
  clientNodes.Create(5);

  // Create point-to-point helper
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

  // Create channels and connect clients to the server
  NetDeviceContainer clientDevices;
  NetDeviceContainer serverDevices;
  for (int i = 0; i < 5; ++i) {
    NetDeviceContainer link = pointToPoint.Install(clientNodes.Get(i), serverNode.Get(0));
    clientDevices.Add(link.Get(0));
    serverDevices.Add(link.Get(1));
  }

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(serverNode);
  internet.Install(clientNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer serverInterfaces = address.Assign(serverDevices);
  Ipv4InterfaceContainer clientInterfaces;
  for (int i = 0; i < 5; ++i) {
    address.NewNetwork();
    NetDeviceContainer devices;
    devices.Add(clientDevices.Get(i));
    clientInterfaces.Add(address.Assign(devices).Get(0));
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create applications: OnOffApplication (clients) and PacketSink (server)
  uint16_t port = 50000;
  OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address(InetSocketAddress(serverInterfaces.GetAddress(0), port)));
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("DataRate", StringValue("1Mbps"));

  ApplicationContainer clientApps;
  for (int i = 0; i < 5; ++i) {
      clientApps.Add(onOffHelper.Install(clientNodes.Get(i)));
  }
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(simulationTime + 1));

  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer serverApp = sinkHelper.Install(serverNode.Get(0));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(simulationTime + 1));

  // Enable Tracing for PCAP
  //pointToPoint.EnablePcapAll("starNetwork");

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Enable NetAnim
  AnimationInterface anim("star-network.xml");
  anim.SetConstantPosition(serverNode.Get(0), 100, 100);
  for (int i = 0; i < 5; ++i) {
      anim.SetConstantPosition(clientNodes.Get(i), 50 * (i + 1), 200);
  }

  // Run simulation
  Simulator::Stop(Seconds(simulationTime + 2));
  Simulator::Run();

  // Print per flow statistics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("star-network.flowmon", true, true);

  Simulator::Destroy();
  return 0;
}