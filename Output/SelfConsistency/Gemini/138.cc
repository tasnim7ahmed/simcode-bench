#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoSimulation");

int main(int argc, char *argv[]) {
  LogComponent::SetAttribute("UdpEchoClientApplication", "PacketSize", UintegerValue(1024));
  LogComponent::SetAttribute("UdpEchoClientApplication", "MaxPackets", UintegerValue(10));

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NS_LOG_INFO("Creating nodes.");
  NodeContainer nodes;
  nodes.Create(4);

  NS_LOG_INFO("Creating channel.");
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer devices23 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));

  NS_LOG_INFO("Assigning IP Addresses.");
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  NS_LOG_INFO("Create Applications.");
  UdpEchoServerHelper echoServer(9);

  ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces23.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  NS_LOG_INFO("Enable Tracing.");
  pointToPoint.EnablePcapAll("udp-echo");

  NS_LOG_INFO("Run Simulation.");
  Simulator::Run();
  Simulator::Destroy();
  NS_LOG_INFO("Done.");

  return 0;
}