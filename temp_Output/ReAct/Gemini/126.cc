#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/dce-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer routers;
  routers.Create(3);

  NodeContainer subnet1Nodes;
  subnet1Nodes.Create(1);

  NodeContainer subnet2Nodes;
  subnet2Nodes.Create(1);

  NodeContainer subnet3Nodes;
  subnet3Nodes.Create(1);

  NodeContainer subnet4Nodes;
  subnet4Nodes.Create(1);

  InternetStackHelper internet;
  internet.Install(routers);
  internet.Install(subnet1Nodes);
  internet.Install(subnet2Nodes);
  internet.Install(subnet3Nodes);
  internet.Install(subnet4Nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer router0Router1 = p2p.Install(routers.Get(0), routers.Get(1));
  NetDeviceContainer router1Router2 = p2p.Install(routers.Get(1), routers.Get(2));

  NetDeviceContainer subnet1Router0 = p2p.Install(subnet1Nodes.Get(0), routers.Get(0));
  NetDeviceContainer subnet2Router0 = p2p.Install(subnet2Nodes.Get(0), routers.Get(0));
  NetDeviceContainer subnet3Router1 = p2p.Install(subnet3Nodes.Get(0), routers.Get(1));
  NetDeviceContainer subnet4Router2 = p2p.Install(subnet4Nodes.Get(0), routers.Get(2));

  Ipv4AddressHelper ipv4;

  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign(router0Router1);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign(router1Router2);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer s1r0 = ipv4.Assign(subnet1Router0);

  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer s2r0 = ipv4.Assign(subnet2Router0);

  ipv4.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer s3r1 = ipv4.Assign(subnet3Router1);

  ipv4.SetBase("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer s4r2 = ipv4.Assign(subnet4Router2);

  Ipv4Address subnet1Address = s1r0.GetAddress(0);
  Ipv4Address subnet2Address = s2r0.GetAddress(0);
  Ipv4Address subnet3Address = s3r1.GetAddress(0);
  Ipv4Address subnet4Address = s4r2.GetAddress(0);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(subnet4Nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(subnet4Address, port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(subnet1Nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  p2p.EnablePcapAll("dvr");

  Simulator::Stop(Seconds(11.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
  }

  monitor->SerializeToXmlFile("dvr_flowmon.xml", true, true);

  Simulator::Destroy();

  return 0;
}