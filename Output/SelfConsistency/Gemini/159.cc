#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridStarBus");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetAttribute("UdpEchoClientApplication", "MaxPackets", UintegerValue(100));

  // Create nodes
  NodeContainer starNodes;
  starNodes.Create(4); // Nodes 0, 1, 2, 3 (star)

  NodeContainer busNodes;
  busNodes.Create(3); // Nodes 4, 5, 6 (bus)

  // Create point-to-point links for the star topology
  PointToPointHelper starPointToPoint;
  starPointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  starPointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer starDevices1 = starPointToPoint.Install(starNodes.Get(0), starNodes.Get(1));
  NetDeviceContainer starDevices2 = starPointToPoint.Install(starNodes.Get(0), starNodes.Get(2));
  NetDeviceContainer starDevices3 = starPointToPoint.Install(starNodes.Get(0), starNodes.Get(3));

  // Create point-to-point links for the bus topology
  PointToPointHelper busPointToPoint;
  busPointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  busPointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer busDevices1 = busPointToPoint.Install(busNodes.Get(0), busNodes.Get(1));
  NetDeviceContainer busDevices2 = busPointToPoint.Install(busNodes.Get(1), busNodes.Get(2));

  // Install the Internet stack on all nodes
  InternetStackHelper internet;
  internet.Install(starNodes);
  internet.Install(busNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer starInterfaces1 = address.Assign(starDevices1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer starInterfaces2 = address.Assign(starDevices2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer starInterfaces3 = address.Assign(starDevices3);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer busInterfaces1 = address.Assign(busDevices1);

  address.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer busInterfaces2 = address.Assign(busDevices2);

  Ipv4InterfaceContainer allInterfaces;
  allInterfaces.Add(starInterfaces1);
  allInterfaces.Add(starInterfaces2);
  allInterfaces.Add(starInterfaces3);
  allInterfaces.Add(busInterfaces1);
  allInterfaces.Add(busInterfaces2);


  // Install UDP echo server on Node 0
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(starNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(19.0));

  // Install UDP echo clients on Nodes 4 and 5
  UdpEchoClientHelper echoClient1(allInterfaces.GetAddress(7), 9); // Node 4
  echoClient1.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps1 = echoClient1.Install(busNodes.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(18.0));

  UdpEchoClientHelper echoClient2(allInterfaces.GetAddress(7), 9); // Node 5 - sending to node 0.
  echoClient2.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps2 = echoClient2.Install(busNodes.Get(1));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(18.0));

  // Enable global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Install FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run the simulation
  Simulator::Stop(Seconds(20.0));
  Simulator::Run();

  // Analyze results
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
    NS_LOG_UNCOND("  Tx Packets: " << i->second.txPackets);
    NS_LOG_UNCOND("  Rx Packets: " << i->second.rxPackets);
    NS_LOG_UNCOND("  Lost Packets: " << i->second.lostPackets);
    NS_LOG_UNCOND("  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 << " kbps");
  }

  Simulator::Destroy();

  return 0;
}