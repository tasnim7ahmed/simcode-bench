#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  // Create nodes: A, B, C
  Ptr<Node> nodeA = CreateObject<Node>();
  Ptr<Node> nodeB = CreateObject<Node>();
  Ptr<Node> nodeC = CreateObject<Node>();

  NodeContainer nodesAB(nodeA, nodeB);
  NodeContainer nodesBC(nodeB, nodeC);
  NodeContainer nodeAcon(nodeA);
  NodeContainer nodeCcon(nodeC);

  // Point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer devAB = p2p.Install(nodesAB);
  NetDeviceContainer devBC = p2p.Install(nodesBC);

  // CSMA devices for A and C
  CsmaHelper csma;
  NetDeviceContainer devAcsma = csma.Install(nodeAcon);
  NetDeviceContainer devCcsma = csma.Install(nodeCcon);

  // Internet stack
  InternetStackHelper internet;
  internet.Install(nodeA);
  internet.Install(nodeB);
  internet.Install(nodeC);

  // IP addressing
  // A-B link: x.x.x.0/30 (use 10.0.0.0/30)
  Ipv4AddressHelper ipv4ab;
  ipv4ab.SetBase("10.0.0.0", "255.255.255.252");
  Ipv4InterfaceContainer ifaceAB = ipv4ab.Assign(devAB);

  // B-C link: y.y.y.0/30 (use 10.0.0.4/30)
  Ipv4AddressHelper ipv4bc;
  ipv4bc.SetBase("10.0.0.4", "255.255.255.252");
  Ipv4InterfaceContainer ifaceBC = ipv4bc.Assign(devBC);

  // A csma: 172.16.1.1/32
  Ipv4AddressHelper ipv4ac;
  ipv4ac.SetBase("172.16.1.1", "255.255.255.255");
  Ipv4InterfaceContainer ifaceAcsma = ipv4ac.Assign(devAcsma);

  // C csma: 192.168.1.1/32
  Ipv4AddressHelper ipv4cc;
  ipv4cc.SetBase("192.168.1.1", "255.255.255.255");
  Ipv4InterfaceContainer ifaceCcsma = ipv4cc.Assign(devCcsma);

  // Assign router B a loopback (required for global routing)
  Ipv4StaticRoutingHelper routing;
  Ptr<Ipv4StaticRouting> staticRoutingB = routing.GetStaticRouting(nodeB->GetObject<Ipv4>());
  nodeB->GetObject<Ipv4>()->AddInterface(CreateObject<LoopbackNetDevice>());
  nodeB->GetObject<Ipv4>()->AddAddress(3, Ipv4InterfaceAddress(Ipv4Address("127.0.0.1"), Ipv4Mask("255.255.255.255")));

  // Enable global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // UDP traffic: from A(csma interface) to C(csma interface)
  uint16_t sinkPort = 9000;

  // Packet sink on C
  Address sinkAddress(InetSocketAddress(ifaceCcsma.GetAddress(0), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodeC);
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  // UDP Client on A
  OnOffHelper clientHelper("ns3::UdpSocketFactory", sinkAddress);
  clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("500kbps")));
  clientHelper.SetAttribute("PacketSize", UintegerValue(512));
  clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  clientHelper.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
  clientHelper.Install(nodeA);

  // Enable traces and pcaps
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll(ascii.CreateFileStream("three-router.tr"));
  p2p.EnablePcapAll("three-router-p2p");
  csma.EnablePcap("three-router-A-csma", devAcsma.Get(0), true);
  csma.EnablePcap("three-router-C-csma", devCcsma.Get(0), true);

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}