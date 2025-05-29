#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ospf-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfExample");

int main(int argc, char *argv[]) {

  bool verbose = false;
  bool printRoutingTables = true;
  std::string animFile = "ospf-example.xml";

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application containers to log if true", verbose);
  cmd.AddValue("printRoutingTables", "Print routing tables if true", printRoutingTables);
  cmd.AddValue("animFile", "File Name for Animation Output", animFile);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("OspfExample", LOG_LEVEL_INFO);
    LogComponentEnable("OspfRouting", LOG_LEVEL_ALL);
    LogComponentEnable("Packet::EnablePrinting", LOG_LEVEL_ALL);
  }

  // Create Nodes
  NodeContainer nodes;
  nodes.Create(4);

  // Create PointToPoint links between nodes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices13 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer devices24 = pointToPoint.Install(nodes.Get(1), nodes.Get(3));
  NetDeviceContainer devices34 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));

  // Install Internet Stack
  InternetStackHelper internet;
  OspfHelper ospfRouting;
  internet.SetRoutingHelper(ospfRouting);
  internet.Install(nodes);

  // Assign IPv4 Addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = ipv4.Assign(devices12);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces13 = ipv4.Assign(devices13);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces24 = ipv4.Assign(devices24);

  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces34 = ipv4.Assign(devices34);

  // Create an OnOff application to generate traffic
  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces34.GetAddress(1), 9)));
  onoff.SetConstantRate(DataRate("1Mbps"));
  ApplicationContainer app = onoff.Install(nodes.Get(0));
  app.Start(Seconds(1.0));
  app.Stop(Seconds(10.0));

  // Create a sink application to receive traffic
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
  ApplicationContainer sinkApp = sink.Install(nodes.Get(3));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));


  // Enable FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();


  // Start Simulation
  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  // Print Routing Tables (optional)
  if (printRoutingTables) {
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
      Ptr<Node> node = nodes.Get(i);
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
      if (ipv4 != nullptr) {
        std::cout << "Routing table for Node " << i << ":" << std::endl;
        Ptr<Ipv4RoutingTable> routingTable = ipv4->GetRoutingTable();
        if (routingTable != nullptr) {
          routingTable->PrintRoutingTable(std::cout);
        } else {
          std::cout << "No routing table found." << std::endl;
        }
        std::cout << std::endl;
      }
    }
  }

  // Analyze and print flow statistics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Delay Sum:  " << i->second.delaySum << "\n";
  }


  Simulator::Destroy();

  return 0;
}