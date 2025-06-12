#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiredNetworkSimulation");

class PacketCounter {
public:
  PacketCounter() : m_tx(0), m_rx(0) {}

  void Tx(Ptr<const Packet> packet) {
    m_tx++;
    NS_LOG_UNCOND("TX: " << m_tx);
  }

  void Rx(Ptr<const Packet> packet) {
    m_rx++;
    NS_LOG_UNCOND("RX: " << m_rx);
  }

private:
  uint32_t m_tx;
  uint32_t m_rx;
};

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("WiredNetworkSimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer dev1, dev2;
  dev1 = p2p.Install(nodes.Get(0), nodes.Get(1));
  dev2 = p2p.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if1 = address.Assign(dev1);
  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if2 = address.Assign(dev2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 8080;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));
  OnOffHelper client("ns3::TcpSocketFactory", Address());
  client.SetAttribute("Remote", AddressValue(InetSocketAddress(if2.GetAddress(1), port)));
  client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  client.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  PacketCounter counters;
  dev1.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&PacketCounter::Tx, &counters));
  dev1.Get(1)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&PacketCounter::Rx, &counters));
  dev2.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&PacketCounter::Tx, &counters));
  dev2.Get(1)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&PacketCounter::Rx, &counters));

  p2p.EnablePcapAll("wired_network_simulation");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}