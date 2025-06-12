#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BasicP2PTopology");

class PacketCounter {
public:
  PacketCounter() : tx(0), rx(0) {}

  void Tx(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket) {
    tx++;
  }

  void Rx(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket) {
    rx++;
  }

  uint32_t GetTxCount() const { return tx; }
  uint32_t GetRxCount() const { return rx; }

private:
  uint32_t tx;
  uint32_t rx;
};

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

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

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(if2.GetAddress(1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(2));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
  onoff.SetConstantRate(DataRate("1Mbps"), 512);
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  p2p.EnablePcapAll("basic-p2p-topology");

  PacketCounter pc0, pc1, pc2;

  Config::Connect("/NodeList/0/ApplicationList/0/Tx", MakeCallback(&PacketCounter::Tx, &pc0));
  Config::Connect("/NodeList/0/ApplicationList/0/Rx", MakeCallback(&PacketCounter::Rx, &pc0));

  Config::Connect("/NodeList/1/ApplicationList/0/Tx", MakeCallback(&PacketCounter::Tx, &pc1));
  Config::Connect("/NodeList/1/ApplicationList/0/Rx", MakeCallback(&PacketCounter::Rx, &pc1));

  Config::Connect("/NodeList/2/ApplicationList/0/Tx", MakeCallback(&PacketCounter::Tx, &pc2));
  Config::Connect("/NodeList/2/ApplicationList/0/Rx", MakeCallback(&PacketCounter::Rx, &pc2));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  std::cout << "Node 0: TX = " << pc0.GetTxCount() << ", RX = " << pc0.GetRxCount() << std::endl;
  std::cout << "Node 1: TX = " << pc1.GetTxCount() << ", RX = " << pc1.GetRxCount() << std::endl;
  std::cout << "Node 2: TX = " << pc2.GetTxCount() << ", RX = " << pc2.GetRxCount() << std::endl;

  return 0;
}