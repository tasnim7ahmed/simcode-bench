#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiredNetworkSimulation");

class PacketCounter {
public:
  PacketCounter() : m_tx(0), m_rx(0) {}

  void TxCallback(Ptr<const Packet> packet) {
    m_tx++;
    NS_LOG_UNCOND("TX Packet Size: " << packet->GetSize());
  }

  void RxCallback(Ptr<const NetDevice>, Ptr<const Packet> packet, uint16_t) {
    m_rx++;
    NS_LOG_UNCOND("RX Packet Size: " << packet->GetSize());
  }

  uint32_t GetTxPackets() const { return m_tx; }
  uint32_t GetRxPackets() const { return m_rx; }

private:
  uint32_t m_tx;
  uint32_t m_rx;
};

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("WiredNetworkSimulation", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(3);

  // Create point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // Links: node0 <-> node1 and node1 <-> node2
  NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

  address.NewNetwork();
  Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

  // Set up global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Set up traffic from node0 to node2
  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(interfaces12.GetAddress(1), sinkPort));

  // Install packet sink on node2
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(2));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  // Install TCP sender on node0
  OnOffHelper onOffHelper("ns3::TcpSocketFactory", sinkAddress);
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("DataRate", StringValue("1Mbps"));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer sourceApps = onOffHelper.Install(nodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(10.0));

  // Enable pcap tracing
  p2p.EnablePcapAll("wired-network");

  // Setup packet counters
  PacketCounter counter0, counter1a, counter1b, counter2;

  devices01.Get(0)->TraceConnectWithoutContext("PhyTxDrop", MakeCallback(&PacketCounter::TxCallback, &counter0));
  devices01.Get(1)->TraceConnectWithoutContext("PhyRxDrop", MakeCallback(&PacketCounter::RxCallback, &counter1a));

  devices12.Get(0)->TraceConnectWithoutContext("PhyTxDrop", MakeCallback(&PacketCounter::TxCallback, &counter1b));
  devices12.Get(1)->TraceConnectWithoutContext("PhyRxDrop", MakeCallback(&PacketCounter::RxCallback, &counter2));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  NS_LOG_INFO("Node 0 transmitted packets: " << counter0.GetTxPackets());
  NS_LOG_INFO("Node 1 received packets (from node0): " << counter1a.GetRxPackets());
  NS_LOG_INFO("Node 1 transmitted packets (to node2): " << counter1b.GetTxPackets());
  NS_LOG_INFO("Node 2 received packets: " << counter2.GetRxPackets());

  Simulator::Destroy();
  return 0;
}