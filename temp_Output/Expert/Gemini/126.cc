#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dvdv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable ("DvdvRoutingProtocol", LOG_LEVEL_ALL);
  LogComponentEnable ("Ipv4RoutingTable", LOG_LEVEL_ALL);

  NodeContainer routers;
  routers.Create(3);

  NodeContainer subnet1Nodes, subnet2Nodes;
  subnet1Nodes.Create(2);
  subnet2Nodes.Create(2);

  NodeContainer allNodes;
  allNodes.Add(routers);
  allNodes.Add(subnet1Nodes);
  allNodes.Add(subnet2Nodes);

  InternetStackHelper internet;
  internet.Install(allNodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer routerDevices[3];
  NetDeviceContainer subnet1Devices, subnet2Devices;

  routerDevices[0] = p2p.Install(routers.Get(0), routers.Get(1));
  routerDevices[1] = p2p.Install(routers.Get(0), routers.Get(2));

  PointToPointHelper subnetLink;
  subnetLink.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  subnetLink.SetChannelAttribute("Delay", StringValue("5ms"));

  subnet1Devices = subnetLink.Install(subnet1Nodes.Get(0), routers.Get(1));
  subnet2Devices = subnetLink.Install(subnet2Nodes.Get(0), routers.Get(2));

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer routerInterfaces[3];
  Ipv4InterfaceContainer subnet1Interfaces, subnet2Interfaces;

  address.SetBase("10.1.1.0", "255.255.255.0");
  routerInterfaces[0] = address.Assign(routerDevices[0]);

  address.SetBase("10.1.2.0", "255.255.255.0");
  routerInterfaces[1] = address.Assign(routerDevices[1]);

  address.SetBase("10.1.3.0", "255.255.255.0");
  subnet1Interfaces = address.Assign(subnet1Devices);

  address.SetBase("10.1.4.0", "255.255.255.0");
  subnet2Interfaces = address.Assign(subnet2Devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  DvdvHelper dvdv;
  dvdv.Install(routers);

  Packet::EnablePrinting();

  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(subnet2Nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(subnet2Interfaces.GetAddress(0), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(subnet1Nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  p2p.EnablePcapAll("dvr", false);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}