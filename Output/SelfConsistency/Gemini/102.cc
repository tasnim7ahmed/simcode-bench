#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/multicast-routing-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MulticastExample");

int main(int argc, char *argv[]) {
  // Enable logging
  LogComponent::Enable("MulticastExample", LOG_LEVEL_INFO);

  // Set simulation parameters
  uint32_t packetSize = 1024;
  std::string dataRate = "1Mbps";
  std::string startTime = "1.0s";
  std::string stopTime = "10.0s";

  // Allow command-line arguments to override defaults
  CommandLine cmd;
  cmd.AddValue("packetSize", "Size of packets to send", packetSize);
  cmd.AddValue("dataRate", "Data rate of the OnOff application", dataRate);
  cmd.Parse(argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(5); // A, B, C, D, E

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Create point-to-point helper
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // Create links
  NetDeviceContainer devicesAB = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devicesAC = p2p.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer devicesAD = p2p.Install(nodes.Get(0), nodes.Get(3));
  NetDeviceContainer devicesAE = p2p.Install(nodes.Get(0), nodes.Get(4));

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAC = address.Assign(devicesAC);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAD = address.Assign(devicesAD);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAE = address.Assign(devicesAE);

  // Enable multicast routing
  MulticastRoutingHelper multicast;
  multicast.Install(nodes);

  // Populate routing tables (static routes, no global routing)
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create multicast address
  Ipv4Address multicastAddress("225.1.2.3");
  Ipv4StaticRoutingHelper staticRouting;
  staticRouting.AddMulticastRoute(nodes.Get(1), multicastAddress, interfacesAB.GetAddress(1));
  staticRouting.AddMulticastRoute(nodes.Get(2), multicastAddress, interfacesAC.GetAddress(1));
  staticRouting.AddMulticastRoute(nodes.Get(3), multicastAddress, interfacesAD.GetAddress(1));
  staticRouting.AddMulticastRoute(nodes.Get(4), multicastAddress, interfacesAE.GetAddress(1));

  // Create OnOff application (sender)
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(multicastAddress, 9));
  onoff.SetConstantRate(DataRate(dataRate));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer appSource = onoff.Install(nodes.Get(0));
  appSource.Start(Seconds(1.0));
  appSource.Stop(Seconds(10.0));

  // Create packet sinks (receivers)
  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
  ApplicationContainer sinkAppsB = sinkHelper.Install(nodes.Get(1));
  ApplicationContainer sinkAppsC = sinkHelper.Install(nodes.Get(2));
  ApplicationContainer sinkAppsD = sinkHelper.Install(nodes.Get(3));
  ApplicationContainer sinkAppsE = sinkHelper.Install(nodes.Get(4));

  sinkAppsB.Start(Seconds(0.0));
  sinkAppsB.Stop(Seconds(11.0));
  sinkAppsC.Start(Seconds(0.0));
  sinkAppsC.Stop(Seconds(11.0));
  sinkAppsD.Start(Seconds(0.0));
  sinkAppsD.Stop(Seconds(11.0));
  sinkAppsE.Start(Seconds(0.0));
  sinkAppsE.Stop(Seconds(11.0));

  // Set up PCAP tracing
  p2p.EnablePcapAll("multicast");

  // Run simulation
  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}