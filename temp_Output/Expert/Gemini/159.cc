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

  NodeContainer starNodes;
  starNodes.Create(4);

  NodeContainer busNodes;
  busNodes.Create(3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer starDevices1 = pointToPoint.Install(starNodes.Get(0), starNodes.Get(1));
  NetDeviceContainer starDevices2 = pointToPoint.Install(starNodes.Get(0), starNodes.Get(2));
  NetDeviceContainer starDevices3 = pointToPoint.Install(starNodes.Get(0), starNodes.Get(3));

  NetDeviceContainer busDevices1 = pointToPoint.Install(busNodes.Get(0), busNodes.Get(1));
  NetDeviceContainer busDevices2 = pointToPoint.Install(busNodes.Get(1), busNodes.Get(2));

  InternetStackHelper stack;
  stack.InstallAll();

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

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(starNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(19.0));

  UdpEchoClientHelper echoClient1(starInterfaces1.GetAddress(0), 9);
  echoClient1.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient1.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps1 = echoClient1.Install(busNodes.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(18.0));

  UdpEchoClientHelper echoClient2(starInterfaces1.GetAddress(0), 9);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient2.SetAttribute("Interval", TimeValue(MilliSeconds(15)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApps2 = echoClient2.Install(busNodes.Get(1));
  clientApps2.Start(Seconds(3.0));
  clientApps2.Stop(Seconds(17.0));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(20));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto it = stats.begin(); it != stats.end(); ++it) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
    std::cout << "Flow " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << it->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << it->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << it->second.lostPackets << "\n";
    std::cout << "  Delay Sum: " << it->second.delaySum << "\n";
  }

  Simulator::Destroy();
  return 0;
}