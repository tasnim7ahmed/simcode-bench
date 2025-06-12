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
  PacketCounter() : txCount(0), rxCount(0) {}

  void TxCallback(Ptr<const Packet> packet) {
    txCount++;
    std::cout << "Packet transmitted, total TX: " << txCount << std::endl;
  }

  void RxCallback(Ptr<const Packet> packet) {
    rxCount++;
    std::cout << "Packet received, total RX: " << rxCount << std::endl;
  }

private:
  uint64_t txCount;
  uint64_t rxCount;
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

  NetDeviceContainer devices1, devices2;
  devices1 = p2p.Install(nodes.Get(0), nodes.Get(1));
  devices2 = p2p.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(interfaces2.GetAddress(1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
  clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  PacketCounter counters;

  nodes.Get(0)->GetDevice(0)->TraceConnectWithoutContext("PhyRxDrop", MakeCallback(&PacketCounter::TxCallback, &counters));
  nodes.Get(0)->GetDevice(0)->TraceConnectWithoutContext("PhyTxDrop", MakeCallback(&PacketCounter::RxCallback, &counters));

  nodes.Get(1)->GetDevice(0)->TraceConnectWithoutContext("PhyRxDrop", MakeCallback(&PacketCounter::TxCallback, &counters));
  nodes.Get(1)->GetDevice(0)->TraceConnectWithoutContext("PhyTxDrop", MakeCallback(&PacketCounter::RxCallback, &counters));

  nodes.Get(1)->GetDevice(1)->TraceConnectWithoutContext("PhyRxDrop", MakeCallback(&PacketCounter::TxCallback, &counters));
  nodes.Get(1)->GetDevice(1)->TraceConnectWithoutContext("PhyTxDrop", MakeCallback(&PacketCounter::RxCallback, &counters));

  nodes.Get(2)->GetDevice(0)->TraceConnectWithoutContext("PhyRxDrop", MakeCallback(&PacketCounter::TxCallback, &counters));
  nodes.Get(2)->GetDevice(0)->TraceConnectWithoutContext("PhyTxDrop", MakeCallback(&PacketCounter::RxCallback, &counters));

  p2p.EnablePcapAll("wired_network_simulation");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}