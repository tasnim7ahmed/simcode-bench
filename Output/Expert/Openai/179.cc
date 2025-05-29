#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

void
PacketTrace(std::string context, Ptr<const Packet> packet)
{
  std::cout << Simulator::Now().GetSeconds() << "s " << context << " Packet UID: " << packet->GetUid()
            << " Size: " << packet->GetSize() << " bytes" << std::endl;
}

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Configure Point-to-Point link
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // Install devices and channels
  NetDeviceContainer devices = p2p.Install(nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Set TCP NewReno
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

  // Install BulkSendApplication on node 0
  uint16_t port = 50000;
  BulkSendHelper bulkSender("ns3::TcpSocketFactory",
                            InetSocketAddress(interfaces.GetAddress(1), port));
  bulkSender.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
  ApplicationContainer sourceApps = bulkSender.Install(nodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(10.0));

  // Install PacketSink on node 1
  PacketSinkHelper sink("ns3::TcpSocketFactory",
                        InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.5));
  sinkApps.Stop(Seconds(10.5));

  // Enable pcap tracing
  p2p.EnablePcapAll("p2p-tcp-newreno");

  // Packet event tracing: Tx/Rx
  Config::Connect("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/PhyTxEnd", MakeCallback(&PacketTrace));
  Config::Connect("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/PhyRxEnd", MakeCallback(&PacketTrace));

  // NetAnim setup
  AnimationInterface anim("p2p-tcp-newreno.xml");
  anim.SetConstantPosition(nodes.Get(0), 10.0, 30.0);
  anim.SetConstantPosition(nodes.Get(1), 60.0, 30.0);

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}