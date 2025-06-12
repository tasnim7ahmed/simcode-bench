#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyExample");

int main(int argc, char *argv[])
{
  LogComponentEnable("TcpL4Protocol", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

  // Create the central node (server) and 5 client nodes
  NodeContainer centralNode;
  centralNode.Create(1);

  NodeContainer clientNodes;
  clientNodes.Create(5);

  // Set up point-to-point links between the central node and each client
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer serverDevices, clientDevices;
  for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
  {
    NodeContainer link = NodeContainer(centralNode.Get(0), clientNodes.Get(i));
    NetDeviceContainer devices = p2p.Install(link);
    serverDevices.Add(devices.Get(0));
    clientDevices.Add(devices.Get(1));
  }

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(centralNode);
  stack.Install(clientNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer serverInterfaces;
  serverInterfaces = address.Assign(serverDevices);

  Ipv4InterfaceContainer clientInterfaces;
  clientInterfaces = address.Assign(clientDevices);

  // Create a TCP server on the central node
  uint16_t port = 9; // Well-known port for TCP
  Address serverAddress(InetSocketAddress(serverInterfaces.GetAddress(0), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer serverApp = sinkHelper.Install(centralNode.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Create TCP client applications on each client node
  OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
  clientHelper.SetAttribute("DataRate", StringValue("2Mbps"));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
  {
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(2.0 + i)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    clientApps.Add(clientHelper.Install(clientNodes.Get(i)));
  }

  // Enable Flow Monitor for performance analysis
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Set up NetAnim for visualization
  AnimationInterface anim("star_topology.xml");
  anim.SetConstantPosition(centralNode.Get(0), 50, 50);
  for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
  {
    anim.SetConstantPosition(clientNodes.Get(i), 20 + i * 20, 20);
  }

  // Run the simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Flow Monitor analysis
  monitor->SerializeToXmlFile("star_flowmon.xml", true, true);

  Simulator::Destroy();
  return 0;
}

