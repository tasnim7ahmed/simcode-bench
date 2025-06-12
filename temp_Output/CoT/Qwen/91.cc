#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceRoutingSimulation");

class PacketLogger {
public:
  void LogPacket(Ptr<const Packet> packet, const Address& from, const Address& to) {
    std::cout << Simulator::Now().GetSeconds() << "s | Packet received: "
              << *packet << " From: " << from << " To: " << to << std::endl;
  }
};

void ReceivePacket(Ptr<Socket> socket) {
  Ptr<Packet> packet;
  while ((packet = socket->Recv())) {
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s | Received packet at socket");
  }
}

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("PacketLogger", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper p2p1, p2p2, p2p3, p2p4;
  p2p1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p1.SetChannelAttribute("Delay", StringValue("2ms"));

  p2p2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p2.SetChannelAttribute("Delay", StringValue("2ms"));

  p2p3.SetDeviceAttribute("DataRate", StringValue("10Mbps")); // Lower metric path
  p2p3.SetChannelAttribute("Delay", StringValue("1ms"));

  p2p4.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p4.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices1 = p2p1.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices2 = p2p2.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer devices3 = p2p3.Install(nodes.Get(1), nodes.Get(3));
  NetDeviceContainer devices4 = p2p4.Install(nodes.Get(2), nodes.Get(3));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);

  Ipv4StaticRoutingHelper routingHelper;

  Ptr<Ipv4StaticRouting> sourceRouting = routingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
  sourceRouting->AddHostRouteTo(interfaces3.GetAddress(1), interfaces1.Get(0), 1); // Metric 1
  sourceRouting->AddHostRouteTo(interfaces4.GetAddress(1), interfaces2.Get(0), 10); // Metric 10

  Ptr<Ipv4StaticRouting> router1Routing = routingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
  router1Routing->AddHostRouteTo(interfaces4.GetAddress(1), interfaces3.Get(1), 1);

  Ptr<Ipv4StaticRouting> router2Routing = routingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
  router2Routing->AddHostRouteTo(interfaces3.GetAddress(1), interfaces4.Get(1), 1);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces3.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> sourceSocket1 = Socket::CreateSocket(nodes.Get(0), tid);
  InetSocketAddress local1 = InetSocketAddress(interfaces1.GetAddress(0), 8080);
  sourceSocket1->Bind(local1);

  Ptr<Socket> sourceSocket2 = Socket::CreateSocket(nodes.Get(0), tid);
  InetSocketAddress local2 = InetSocketAddress(interfaces2.GetAddress(0), 8081);
  sourceSocket2->Bind(local2);

  Ptr<Packet> packet1 = Create<Packet>(1024);
  sourceSocket1->SendTo(packet1, 0, InetSocketAddress(interfaces3.GetAddress(1), 9));
  sourceSocket2->SendTo(packet1, 0, InetSocketAddress(interfaces4.GetAddress(1), 9));

  PacketLogger logger;
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoServer/Receive",
                  MakeCallback(&PacketLogger::LogPacket, &logger));

  Config::Connect("/NodeList/0/ApplicationList/*/$ns3::UdpEchoClient/Tx",
                  MakeCallback(&PacketLogger::LogPacket, &logger));

  AsciiTraceHelper ascii;
  p2p1.EnableAsciiAll(ascii.CreateFileStream("multi-interface-routing.tr"));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}