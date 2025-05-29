#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Create nodes
  NodeContainer routers;
  routers.Create(3); // R1, R2, R3

  NodeContainer lanNodes;
  lanNodes.Create(2); // LAN Node 1, LAN Node 2

  NodeContainer destNode;
  destNode.Create(1); // Destination node connected to R3

  // Create point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer r1r2Devices = p2p.Install(routers.Get(0), routers.Get(1));
  NetDeviceContainer r2r3Devices = p2p.Install(routers.Get(1), routers.Get(2));
  NetDeviceContainer r3destDevices = p2p.Install(routers.Get(2), destNode.Get(0));

  // Create LAN
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", StringValue("100us"));

  NetDeviceContainer lanDevices = csma.Install(NodeContainer(lanNodes, routers.Get(1)));

  // Install internet stack
  InternetStackHelper stack;
  stack.InstallAll();

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer r1r2Interfaces = address.Assign(r1r2Devices);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer r2r3Interfaces = address.Assign(r2r3Devices);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer r3destInterfaces = address.Assign(r3destDevices);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer lanInterfaces = address.Assign(lanDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create TCP application
  uint16_t port = 50000;
  Address sinkLocalAddress(InetSocketAddress(r3destInterfaces.GetAddress(0), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(destNode.Get(0));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(r3destInterfaces.GetAddress(0), port));
  bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApp = bulkSendHelper.Install(lanNodes.Get(0));
  sourceApp.Start(Seconds(2.0));
  sourceApp.Stop(Seconds(10.0));

  // Flow monitor
  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  // Run simulation
  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  // Collect statistics
  monitor->CheckForLostPackets();
  std::ofstream file;
  file.open("simulation_results.xml");
  monitor->SerializeToXmlFile("simulation_results.xml", true, true);
  file.close();

  Simulator::Destroy();
  return 0;
}