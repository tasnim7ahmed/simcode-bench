#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LinkStateRoutingSimulation");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetLevel(
      "LinkStateRoutingSimulation",
      LOG_LEVEL_INFO); // Enable INFO level logs for this component

  // Create nodes
  NodeContainer routers;
  routers.Create(4);

  // Configure PointToPoint links
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Create links between routers
  NetDeviceContainer devices01 = pointToPoint.Install(routers.Get(0), routers.Get(1));
  NetDeviceContainer devices02 = pointToPoint.Install(routers.Get(0), routers.Get(2));
  NetDeviceContainer devices13 = pointToPoint.Install(routers.Get(1), routers.Get(3));
  NetDeviceContainer devices23 = pointToPoint.Install(routers.Get(2), routers.Get(3));

  // Install Internet stack on all nodes
  InternetStackHelper internet;
  internet.Install(routers);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces13 = address.Assign(devices13);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

  // Install OSPF routing protocol
  OlsrHelper olsr;
  Ipv4GlobalRoutingHelper::PopulateRoutingTables(); // Needed for OSPF to initialize
  
  // Create a sink application on router 3
  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(interfaces23.GetAddress(1), sinkPort)); // Router 3's IP
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(routers.Get(3));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));
  
  // Create an on-off application on router 0
  OnOffHelper onOffHelper("ns3::TcpSocketFactory", sinkAddress);
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
  onOffHelper.SetAttribute("DataRate", StringValue("1Mbps"));

  ApplicationContainer clientApps = onOffHelper.Install(routers.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Enable PCAP tracing on all devices (optional for LSA visualization)
  pointToPoint.EnablePcapAll("link-state-routing");

  // Install FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run the simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Analyze the flow monitor data
  monitor->CheckForLostPackets();
  Ptr<Iprobe> probe = Create<Iprobe> ();
  monitor->SerializeToXmlFile("link-state-routing.flowmon", true, true, probe);

  Simulator::Destroy();
  return 0;
}