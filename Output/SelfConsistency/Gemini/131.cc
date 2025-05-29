#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CountToInfinity");

int main(int argc, char *argv[]) {
  LogComponent::SetAttribute("Ipv4DistanceVectorRouting", "PeriodicUpdateInterval", TimeValue(Seconds(1)));

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NS_LOG_INFO("Create nodes.");
  NodeContainer nodes;
  nodes.Create(3);
  Names::Add("A", nodes.Get(0));
  Names::Add("B", nodes.Get(1));
  Names::Add("C", nodes.Get(2));

  NS_LOG_INFO("Create channels.");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devicesAtoB = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devicesBtoC = p2p.Install(nodes.Get(1), nodes.Get(2));

  NS_LOG_INFO("Assign IP addresses.");
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAtoB = address.Assign(devicesAtoB);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesBtoC = address.Assign(devicesBtoC);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  NS_LOG_INFO("Create applications.");
  uint16_t port = 9;

  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(20.0));

  UdpEchoClientHelper echoClient(interfacesBtoC.GetAddress(1), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(20.0));

  // Break the link between B and C
  Simulator::Schedule(Seconds(5.0), [&](){
    NS_LOG_INFO("Break link between B and C.");
    devicesBtoC.Get(0)->GetObject<PointToPointNetDevice>()->SetDown();
    devicesBtoC.Get(1)->GetObject<PointToPointNetDevice>()->SetDown();
  });

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}