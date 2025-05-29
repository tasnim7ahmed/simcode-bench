#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(4);

  // Create point-to-point links between each pair
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // Link: node0 <-> node1
  NetDeviceContainer dev01 = p2p.Install(nodes.Get(0), nodes.Get(1));

  // Link: node1 <-> node2
  NetDeviceContainer dev12 = p2p.Install(nodes.Get(1), nodes.Get(2));

  // Link: node2 <-> node3
  NetDeviceContainer dev23 = p2p.Install(nodes.Get(2), nodes.Get(3));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses to each segment
  Ipv4AddressHelper address01, address12, address23;
  address01.SetBase("10.1.1.0", "255.255.255.0");
  address12.SetBase("10.1.2.0", "255.255.255.0");
  address23.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iface01 = address01.Assign(dev01);
  Ipv4InterfaceContainer iface12 = address12.Assign(dev12);
  Ipv4InterfaceContainer iface23 = address23.Assign(dev23);

  // Enable PCAP tracing
  p2p.EnablePcapAll("line-topology");

  // Create UDP server on node 3
  uint16_t port = 4000;
  UdpServerHelper udpServer(port);
  ApplicationContainer serverApp = udpServer.Install(nodes.Get(3));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Create UDP client on node 0, sending to node 3
  UdpClientHelper udpClient(iface23.GetAddress(1), port);
  udpClient.SetAttribute("MaxPackets", UintegerValue(320));
  udpClient.SetAttribute("Interval", TimeValue(Seconds(0.03)));
  udpClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  // Set up routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}