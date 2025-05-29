#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer starNodes;
  starNodes.Create(4);

  NodeContainer busNodes;
  busNodes.Create(3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer starDevices1 = p2p.Install(starNodes.Get(0), starNodes.Get(1));
  NetDeviceContainer starDevices2 = p2p.Install(starNodes.Get(0), starNodes.Get(2));
  NetDeviceContainer starDevices3 = p2p.Install(starNodes.Get(0), starNodes.Get(3));

  NetDeviceContainer busDevices1 = p2p.Install(busNodes.Get(0), busNodes.Get(1));
  NetDeviceContainer busDevices2 = p2p.Install(busNodes.Get(1), busNodes.Get(2));

  InternetStackHelper stack;
  stack.Install(starNodes);
  stack.Install(busNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer starInterfaces1 = address.Assign(starDevices1);
  address.NewNetwork();
  Ipv4InterfaceContainer starInterfaces2 = address.Assign(starDevices2);
  address.NewNetwork();
  Ipv4InterfaceContainer starInterfaces3 = address.Assign(starDevices3);
  address.NewNetwork();
  Ipv4InterfaceContainer busInterfaces1 = address.Assign(busDevices1);
  address.NewNetwork();
  Ipv4InterfaceContainer busInterfaces2 = address.Assign(busDevices2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(starNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(20.0));

  UdpEchoClientHelper echoClient1(starInterfaces1.GetAddress(0), 9);
  echoClient1.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps1 = echoClient1.Install(busNodes.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(20.0));

  UdpEchoClientHelper echoClient2(starInterfaces1.GetAddress(0), 9);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps2 = echoClient2.Install(busNodes.Get(1));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(20.0));


  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(20.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }

  monitor->SerializeToXmlFile("hybrid.flowmon", true, true);

  Simulator::Destroy();
  return 0;
}