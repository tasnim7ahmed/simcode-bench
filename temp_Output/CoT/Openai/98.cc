#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/queue.h"
#include "ns3/traffic-control-module.h"
#include <fstream>

using namespace ns3;

// Trace sink for packet receptions
void
RxTrace(std::string context, Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream rxStream("packet-receive.log", std::ios::app);
  rxStream << Simulator::Now().GetSeconds() << " " << context << " " << packet->GetSize() << std::endl;
}

// Trace sink for queue events
void
QueueTrace(std::string context, Ptr<const QueueDiscItem> item)
{
  static std::ofstream queueStream("queue-activity.log", std::ios::app);
  queueStream << Simulator::Now().GetSeconds() << " " << context << " packetSize " << item->GetPacket()->GetSize() << std::endl;
}

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // 7 Nodes: n0-n6
  NodeContainer nodes;
  nodes.Create(7);

  // Point-to-point links
  // p2p1: n1 <-> n2
  // p2p2: n1 <-> n3
  // p2p3: n6 <-> n4
  // p2p4: n6 <-> n5

  NodeContainer p2p1 = NodeContainer(nodes.Get(1), nodes.Get(2));
  NodeContainer p2p2 = NodeContainer(nodes.Get(1), nodes.Get(3));
  NodeContainer p2p3 = NodeContainer(nodes.Get(6), nodes.Get(4));
  NodeContainer p2p4 = NodeContainer(nodes.Get(6), nodes.Get(5));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer p2p1Devs = p2p.Install(p2p1);
  NetDeviceContainer p2p2Devs = p2p.Install(p2p2);
  NetDeviceContainer p2p3Devs = p2p.Install(p2p3);
  NetDeviceContainer p2p4Devs = p2p.Install(p2p4);

  // CSMA network: n2, n3, n4, n5, n0
  NodeContainer csmaNodes;
  csmaNodes.Add(nodes.Get(0));
  csmaNodes.Add(nodes.Get(2));
  csmaNodes.Add(nodes.Get(3));
  csmaNodes.Add(nodes.Get(4));
  csmaNodes.Add(nodes.Get(5));

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer csmaDevs = csma.Install(csmaNodes);

  InternetStackHelper internet;
  Ipv4GlobalRoutingHelper globalRouting;
  internet.SetRoutingHelper(globalRouting);
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  Ipv4InterfaceContainer if_p2p1, if_p2p2, if_p2p3, if_p2p4, if_csma;

  address.SetBase("10.1.1.0", "255.255.255.0");
  if_p2p1 = address.Assign(p2p1Devs);

  address.SetBase("10.1.2.0", "255.255.255.0");
  if_p2p2 = address.Assign(p2p2Devs);

  address.SetBase("10.1.3.0", "255.255.255.0");
  if_p2p3 = address.Assign(p2p3Devs);

  address.SetBase("10.1.4.0", "255.255.255.0");
  if_p2p4 = address.Assign(p2p4Devs);

  address.SetBase("10.1.5.0", "255.255.255.0");
  if_csma = address.Assign(csmaDevs);

  // Enable global routing on interface up/down events
  Config::SetDefault("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue(true));

  // Application setup
  // UDP Server on node 6
  uint16_t port = 4000;
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(nodes.Get(6));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(25.0));

  // UDP Client on node 1: two flows via the two possible paths
  // First flow: start at t=1s, use p2p1 (n1->n2->CSMA->n4/n5->p2p3/p2p4->n6)
  // Second flow: start at t=11s, use p2p2 (n1->n3->CSMA->n4/n5->p2p3/p2p4->n6)
  // Both stop at t=21s

  // First Flow
  UdpClientHelper client1(if_p2p3.GetAddress(1), port);
  client1.SetAttribute("MaxPackets", UintegerValue(10000));
  client1.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
  client1.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApp1 = client1.Install(nodes.Get(1));
  clientApp1.Start(Seconds(1.0));
  clientApp1.Stop(Seconds(21.0));

  // Second Flow (uses the same destination but traffic will be re-routed based on link status)
  UdpClientHelper client2(if_p2p4.GetAddress(1), port);
  client2.SetAttribute("MaxPackets", UintegerValue(10000));
  client2.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
  client2.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApp2 = client2.Install(nodes.Get(1));
  clientApp2.Start(Seconds(11.0));
  clientApp2.Stop(Seconds(21.0));

  // Schedule Interface events to cause rerouting
  // Down p2p1 (n1 - n2) at 5s, Up at 15s
  Ptr<NetDevice> n1p2p1 = p2p1Devs.Get(0);
  Ptr<NetDevice> n2p2p1 = p2p1Devs.Get(1);
  Simulator::Schedule(Seconds(5.0), &NetDevice::SetReceiveEnable, n1p2p1, false);
  Simulator::Schedule(Seconds(5.0), &NetDevice::SetReceiveEnable, n2p2p1, false);
  Simulator::Schedule(Seconds(15.0), &NetDevice::SetReceiveEnable, n1p2p1, true);
  Simulator::Schedule(Seconds(15.0), &NetDevice::SetReceiveEnable, n2p2p1, true);

  // Down p2p2 (n1 - n3) at 8s, Up at 18s
  Ptr<NetDevice> n1p2p2 = p2p2Devs.Get(0);
  Ptr<NetDevice> n3p2p2 = p2p2Devs.Get(1);
  Simulator::Schedule(Seconds(8.0), &NetDevice::SetReceiveEnable, n1p2p2, false);
  Simulator::Schedule(Seconds(8.0), &NetDevice::SetReceiveEnable, n3p2p2, false);
  Simulator::Schedule(Seconds(18.0), &NetDevice::SetReceiveEnable, n1p2p2, true);
  Simulator::Schedule(Seconds(18.0), &NetDevice::SetReceiveEnable, n3p2p2, true);

  // Down p2p3 (n6 - n4) at 10s, Up at 20s
  Ptr<NetDevice> n6p2p3 = p2p3Devs.Get(0);
  Ptr<NetDevice> n4p2p3 = p2p3Devs.Get(1);
  Simulator::Schedule(Seconds(10.0), &NetDevice::SetReceiveEnable, n6p2p3, false);
  Simulator::Schedule(Seconds(10.0), &NetDevice::SetReceiveEnable, n4p2p3, false);
  Simulator::Schedule(Seconds(20.0), &NetDevice::SetReceiveEnable, n6p2p3, true);
  Simulator::Schedule(Seconds(20.0), &NetDevice::SetReceiveEnable, n4p2p3, true);

  // Down p2p4 (n6 - n5) at 12s, Up at 22s
  Ptr<NetDevice> n6p2p4 = p2p4Devs.Get(0);
  Ptr<NetDevice> n5p2p4 = p2p4Devs.Get(1);
  Simulator::Schedule(Seconds(12.0), &NetDevice::SetReceiveEnable, n6p2p4, false);
  Simulator::Schedule(Seconds(12.0), &NetDevice::SetReceiveEnable, n5p2p4, false);
  Simulator::Schedule(Seconds(22.0), &NetDevice::SetReceiveEnable, n6p2p4, true);
  Simulator::Schedule(Seconds(22.0), &NetDevice::SetReceiveEnable, n5p2p4, true);

  // Connect Rx trace on node 6's UDP socket (UdpServer, all interfaces)
  Ptr<Node> node6 = nodes.Get(6);
  for (uint32_t i = 0; i < node6->GetNDevices(); ++i)
  {
    std::ostringstream oss;
    oss << "/NodeList/" << node6->GetId() << "/DeviceList/" << i << "/$ns3::Ipv4L3Protocol/Rx";
    Config::Connect(oss.str(), MakeCallback(&RxTrace));
  }

  // Trace queue events on all relevant devices (p2p and csma)
  for (NetDeviceContainer::Iterator it = p2p1Devs.Begin(); it != p2p1Devs.End(); ++it)
  {
    std::ostringstream oss;
    oss << (*it)->GetNode()->GetId();
    std::string path = "/NodeList/" + oss.str() + "/DeviceList/" + std::to_string((*it)->GetIfIndex()) + "/$ns3::PointToPointNetDevice/TxQueue/Enqueue";
    Config::Connect(path, MakeCallback(&QueueTrace));
  }
  for (NetDeviceContainer::Iterator it = p2p2Devs.Begin(); it != p2p2Devs.End(); ++it)
  {
    std::ostringstream oss;
    oss << (*it)->GetNode()->GetId();
    std::string path = "/NodeList/" + oss.str() + "/DeviceList/" + std::to_string((*it)->GetIfIndex()) + "/$ns3::PointToPointNetDevice/TxQueue/Enqueue";
    Config::Connect(path, MakeCallback(&QueueTrace));
  }
  for (NetDeviceContainer::Iterator it = p2p3Devs.Begin(); it != p2p3Devs.End(); ++it)
  {
    std::ostringstream oss;
    oss << (*it)->GetNode()->GetId();
    std::string path = "/NodeList/" + oss.str() + "/DeviceList/" + std::to_string((*it)->GetIfIndex()) + "/$ns3::PointToPointNetDevice/TxQueue/Enqueue";
    Config::Connect(path, MakeCallback(&QueueTrace));
  }
  for (NetDeviceContainer::Iterator it = p2p4Devs.Begin(); it != p2p4Devs.End(); ++it)
  {
    std::ostringstream oss;
    oss << (*it)->GetNode()->GetId();
    std::string path = "/NodeList/" + oss.str() + "/DeviceList/" + std::to_string((*it)->GetIfIndex()) + "/$ns3::PointToPointNetDevice/TxQueue/Enqueue";
    Config::Connect(path, MakeCallback(&QueueTrace));
  }
  for (NetDeviceContainer::Iterator it = csmaDevs.Begin(); it != csmaDevs.End(); ++it)
  {
    std::ostringstream oss;
    oss << (*it)->GetNode()->GetId();
    std::string path = "/NodeList/" + oss.str() + "/DeviceList/" + std::to_string((*it)->GetIfIndex()) + "/$ns3::CsmaNetDevice/TxQueue/Enqueue";
    Config::Connect(path, MakeCallback(&QueueTrace));
  }

  Simulator::Stop(Seconds(25.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}