#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mesh-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologyExample");

int main(int argc, char *argv[])
{
  // Log setup
  LogComponentEnable("TcpL4Protocol", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  
  // Part 1: Star topology using TCP
  NodeContainer starNodes;
  starNodes.Create(6); // Central node + 5 client nodes
  
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  
  // Install point-to-point devices between central node and client nodes
  NetDeviceContainer p2pDevices;
  for (uint32_t i = 1; i < starNodes.GetN(); ++i)
  {
    NodeContainer link = NodeContainer(starNodes.Get(0), starNodes.Get(i));
    NetDeviceContainer devices = p2p.Install(link);
    p2pDevices.Add(devices);
  }
  
  // Part 2: Mesh topology using UDP
  NodeContainer meshNodes;
  meshNodes.Create(4); // 4 mesh nodes

  MeshHelper mesh;
  mesh.SetStackInstaller("ns3::Dot11sStack");
  mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
  mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
  mesh.SetNumberOfInterfaces(1);

  NetDeviceContainer meshDevices = mesh.Install(meshNodes);

  // Internet stack
  InternetStackHelper internet;
  internet.Install(starNodes);
  internet.Install(meshNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  address.Assign(p2pDevices);
  
  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces = address.Assign(meshDevices);

  // TCP Application (Star topology)
  uint16_t tcpPort = 9; // Well-known port for TCP
  Address tcpServerAddress(InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
  
  PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", tcpServerAddress);
  ApplicationContainer tcpServerApp = tcpSinkHelper.Install(starNodes.Get(0)); // Central node as server
  tcpServerApp.Start(Seconds(1.0));
  tcpServerApp.Stop(Seconds(20.0));
  
  OnOffHelper tcpClientHelper("ns3::TcpSocketFactory", InetSocketAddress(starNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), tcpPort));
  tcpClientHelper.SetAttribute("DataRate", StringValue("2Mbps"));
  tcpClientHelper.SetAttribute("PacketSize", UintegerValue(1024));
  
  ApplicationContainer tcpClientApps;
  for (uint32_t i = 1; i < starNodes.GetN(); ++i)
  {
    tcpClientHelper.SetAttribute("StartTime", TimeValue(Seconds(2.0 + i)));
    tcpClientHelper.SetAttribute("StopTime", TimeValue(Seconds(20.0)));
    tcpClientApps.Add(tcpClientHelper.Install(starNodes.Get(i)));
  }

  // UDP Application (Mesh topology)
  uint16_t udpPort = 8080; // UDP echo port
  UdpEchoServerHelper udpServer(udpPort);
  ApplicationContainer udpServerApp = udpServer.Install(meshNodes.Get(0)); // First node as server
  udpServerApp.Start(Seconds(1.0));
  udpServerApp.Stop(Seconds(20.0));

  UdpEchoClientHelper udpClient(meshInterfaces.GetAddress(0), udpPort);
  udpClient.SetAttribute("MaxPackets", UintegerValue(100));
  udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  udpClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer udpClientApps;
  for (uint32_t i = 1; i < meshNodes.GetN(); ++i)
  {
    udpClient.SetAttribute("StartTime", TimeValue(Seconds(2.0 + i)));
    udpClient.SetAttribute("StopTime", TimeValue(Seconds(20.0)));
    udpClientApps.Add(udpClient.Install(meshNodes.Get(i)));
  }

  // Flow monitor
  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  // Set up NetAnim for visualization
  AnimationInterface anim("hybrid_topology.xml");
  anim.SetConstantPosition(starNodes.Get(0), 50, 50); // Central node position
  for (uint32_t i = 1; i < starNodes.GetN(); ++i)
  {
    anim.SetConstantPosition(starNodes.Get(i), 20 + i * 10, 50);
  }
  for (uint32_t i = 0; i < meshNodes.GetN(); ++i)
  {
    anim.SetConstantPosition(meshNodes.Get(i), 50, 20 + i * 10);
  }

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  
  monitor->SerializeToXmlFile("hybrid_flowmon.xml", true, true);
  
  Simulator::Destroy();
  return 0;
}

