#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpRouterLanSimulation");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetLevel(LOG_PREFIX "TcpRouterLanSimulation", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer routers;
  routers.Create(3);

  NodeContainer lanNodes;
  lanNodes.Create(2);

  NodeContainer endNodes;
  endNodes.Create(1);

  // Create point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer r1r2Devices = p2p.Install(routers.Get(0), routers.Get(1));
  NetDeviceContainer r2r3Devices = p2p.Install(routers.Get(1), routers.Get(2));

  // Create LAN
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer lanDevices = csma.Install(lanNodes, routers.Get(1));

  // Install end node link
  NetDeviceContainer endNodeDevice = p2p.Install(routers.Get(2), endNodes.Get(0));

  // Install Internet stack
  InternetStackHelper stack;
  stack.InstallAll();

  // Assign addresses
  Ipv4AddressHelper address;

  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer r1r2Interfaces = address.Assign(r1r2Devices);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer r2r3Interfaces = address.Assign(r2r3Devices);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer lanInterfaces = address.Assign(lanDevices);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer endNodeInterface = address.Assign(endNodeDevice);

  // Set up routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create TCP application
  uint16_t port = 50000;
  BulkSendHelper source("ns3::TcpSocketFactory",
                         InetSocketAddress(endNodeInterface.GetAddress(0), port));
  source.SetAttribute("SendSize", UintegerValue(1024));
  source.SetAttribute("MaxBytes", UintegerValue(1000000));
  ApplicationContainer sourceApps = source.Install(lanNodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(endNodes.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  // Create FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Animation
  // comment out this line to disable animation
  AnimationInterface anim("tcp-router-lan.xml");
  anim.SetConstantPosition(routers.Get(0), 10, 20);
  anim.SetConstantPosition(routers.Get(1), 30, 20);
  anim.SetConstantPosition(routers.Get(2), 50, 20);
  anim.SetConstantPosition(lanNodes.Get(0), 25, 30);
  anim.SetConstantPosition(lanNodes.Get(1), 35, 30);
  anim.SetConstantPosition(endNodes.Get(0), 60, 30);

  // Run simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Print FlowMonitor statistics
  monitor->CheckForLostPackets();

  std::ofstream os;
  os.open("tcp-router-lan.flowmon");
  monitor->SerializeToXmlFile("tcp-router-lan.flowmon", true, true);
  os.close();

  Simulator::Destroy();

  return 0;
}