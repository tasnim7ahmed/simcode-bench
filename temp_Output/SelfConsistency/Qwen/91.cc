#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceRoutingSimulation");

class PacketLogger {
public:
  static void LogPacket(Ptr<const Packet> packet, const Address &from, const Address &to) {
    NS_LOG_INFO(Simulator::Now().GetSeconds() << "s Packet received. Size: " << packet->GetSize());
    NS_LOG_INFO("From: " << from << " To: " << to);
  }
};

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("MultiInterfaceRoutingSimulation", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(4); // source (0), router1 (1), router2 (2), destination (3)

  // Create point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices1 = p2p.Install(nodes.Get(0), nodes.Get(1)); // Source <-> Router1
  NetDeviceContainer devices2 = p2p.Install(nodes.Get(0), nodes.Get(2)); // Source <-> Router2
  NetDeviceContainer devices3 = p2p.Install(nodes.Get(1), nodes.Get(3)); // Router1 <-> Destination
  NetDeviceContainer devices4 = p2p.Install(nodes.Get(2), nodes.Get(3)); // Router2 <-> Destination

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

  address.NewNetwork();
  Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

  address.NewNetwork();
  Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);

  address.NewNetwork();
  Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);

  // Set up static routing
  Ipv4StaticRoutingHelper routingHelper;

  Ptr<Ipv4StaticRouting> sourceRouting = routingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
  NS_ASSERT(sourceRouting);

  // Add two routes to destination node (10.1.3.2 and 10.1.4.2)
  Ipv4Address destAddr1 = interfaces3.GetAddress(1);  // Destination via Router1
  Ipv4Address destAddr2 = interfaces4.GetAddress(1);  // Destination via Router2

  // Route through Router1 (lower metric - preferred path)
  sourceRouting->AddHostRouteTo(destAddr2, interfaces1.GetAddress(0), 1, 1); // Metric 1

  // Route through Router2 (higher metric - backup path)
  sourceRouting->AddHostRouteTo(destAddr2, interfaces2.GetAddress(0), 2, 10); // Metric 10

  // Install applications on destination node
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Configure client application
  UdpEchoClientHelper echoClient(interfaces4.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Bind socket dynamically to different interfaces
  Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), TypeId::LookupByName("ns3::UdpSocketFactory"));
  InetSocketAddress remote(interfaces4.GetAddress(1), 9);
  clientSocket->Connect(remote);

  // Set interface based on routing decisions
  clientSocket->BindToNetDevice(nodes.Get(0)->GetDevice(0)); // Initially bind to first interface

  // Setup packet receive callbacks
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoServer/ReceivedPacket",
                  MakeCallback(&PacketLogger::LogPacket));
  Config::Connect("/NodeList/0/ApplicationList/*/$ns3::UdpEchoClient/ReceivedPacket",
                  MakeCallback(&PacketLogger::LogPacket));

  // Enable pcap tracing
  p2p.EnablePcapAll("multi-interface-routing");

  // Run simulation
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}