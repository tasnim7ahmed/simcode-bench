#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ospf-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[4];
  devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1));
  devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2));
  devices[2] = p2p.Install(nodes.Get(2), nodes.Get(3));
  devices[3] = p2p.Install(nodes.Get(3), nodes.Get(0));

  InternetStackHelper internet;
  OspfHelper ospf;
  internet.AddProtocol(&ospf, 0);

  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces[4];
  interfaces[0] = ipv4.Assign(devices[0]);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  interfaces[1] = ipv4.Assign(devices[1]);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  interfaces[2] = ipv4.Assign(devices[2]);

  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  interfaces[3] = ipv4.Assign(devices[3]);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces[2].GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  p2p.EnablePcapAll("ospf-routing");

  Simulator::Stop(Seconds(11.0));

  Simulator::Run();

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    Ptr<Node> node = nodes.Get(i);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    std::cout << "Routing table for node " << i << ":" << std::endl;
    ipv4->GetRoutingProtocol()->PrintRoutingTable(Create<OutputStreamWrapper>("routing_table_" + std::to_string(i) + ".txt", std::ios::out));
  }

  Simulator::Destroy();
  return 0;
}