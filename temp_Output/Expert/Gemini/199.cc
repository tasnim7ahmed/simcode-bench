#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer routers;
  routers.Create(3);

  NodeContainer n0r1 = NodeContainer(routers.Get(0), routers.Get(1));
  NodeContainer n1r2 = NodeContainer(routers.Get(1), routers.Get(2));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer d0r1 = p2p.Install(n0r1);
  NetDeviceContainer d1r2 = p2p.Install(n1r2);

  NodeContainer lanNodes;
  lanNodes.Create(2);

  NodeContainer r2lan = NodeContainer(routers.Get(1), lanNodes.Get(0));

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
  csma.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer d2lan = csma.Install(r2lan);

  InternetStackHelper stack;
  stack.Install(routers);
  stack.Install(lanNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0r1 = address.Assign(d0r1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1r2 = address.Assign(d1r2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2lan = address.Assign(d2lan);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(routers.Get(2));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(i1r2.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(lanNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));

  AnimationInterface anim("topology.xml");
  anim.SetConstantPosition(routers.Get(0), 10, 20);
  anim.SetConstantPosition(routers.Get(1), 50, 20);
  anim.SetConstantPosition(routers.Get(2), 90, 20);
  anim.SetConstantPosition(lanNodes.Get(0), 50, 60);
  anim.SetConstantPosition(lanNodes.Get(1), 50, 80);

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
  }

  monitor->SerializeToXmlFile("simulation.flowmon", true, true);

  Simulator::Destroy();
  return 0;
}