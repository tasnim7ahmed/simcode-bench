#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-ospf-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/on-off-application.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices1 = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices2 = p2p.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer devices3 = p2p.Install(nodes.Get(2), nodes.Get(3));
  NetDeviceContainer devices4 = p2p.Install(nodes.Get(3), nodes.Get(0));

  InternetStackHelper internet;
  OspfHelper ospf;
  internet.SetRoutingHelper(ospf);
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = ipv4.Assign(devices1);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = ipv4.Assign(devices2);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces3 = ipv4.Assign(devices3);

  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces4 = ipv4.Assign(devices4);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces4.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4RoutingProtocol> proto = nodes.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol();
  if (proto)
    {
      std::stringstream routingTableStream;
      proto->PrintRoutingTable(routingTableStream);
      std::cout << "Routing Table Node 0:" << std::endl << routingTableStream.str() << std::endl;
    }
  else {
    std::cout << "No routing protocol found on Node 0" << std::endl;
  }
  proto = nodes.Get(1)->GetObject<Ipv4>()->GetRoutingProtocol();
  if (proto)
    {
      std::stringstream routingTableStream;
      proto->PrintRoutingTable(routingTableStream);
      std::cout << "Routing Table Node 1:" << std::endl << routingTableStream.str() << std::endl;
    }
  else {
    std::cout << "No routing protocol found on Node 1" << std::endl;
  }
   proto = nodes.Get(2)->GetObject<Ipv4>()->GetRoutingProtocol();
  if (proto)
    {
      std::stringstream routingTableStream;
      proto->PrintRoutingTable(routingTableStream);
      std::cout << "Routing Table Node 2:" << std::endl << routingTableStream.str() << std::endl;
    }
  else {
    std::cout << "No routing protocol found on Node 2" << std::endl;
  }
   proto = nodes.Get(3)->GetObject<Ipv4>()->GetRoutingProtocol();
  if (proto)
    {
      std::stringstream routingTableStream;
      proto->PrintRoutingTable(routingTableStream);
      std::cout << "Routing Table Node 3:" << std::endl << routingTableStream.str() << std::endl;
    }
  else {
    std::cout << "No routing protocol found on Node 3" << std::endl;
  }

  monitor->SerializeToXmlFile("ospf.flowmon", true, true);

  Simulator::Destroy();
  return 0;
}