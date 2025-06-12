#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpStarServer");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  int nNodes = 9;
  std::string dataRate = "1Mbps";
  uint32_t packetSize = 1024;

  cmd.AddValue("nNodes", "Number of nodes", nNodes);
  cmd.AddValue("dataRate", "Data rate of CBR traffic", dataRate);
  cmd.AddValue("packetSize", "Size of packets", packetSize);
  cmd.Parse(argc, argv);

  LogComponentEnable("TcpStarServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(nNodes);

  NodeContainer hubNode = NodeContainer(nodes.Get(0));
  NodeContainer spokeNodes;
  for (int i = 1; i < nNodes; ++i) {
    spokeNodes.Add(nodes.Get(i));
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  TrafficControlHelper p2pQueue;
  uint16_t queueDiscId = p2pQueue.Install(p2p.GetDevice());

  NetDeviceContainer hubDevices;
  NetDeviceContainer spokeDevices;

  for (int i = 1; i < nNodes; ++i) {
    NetDeviceContainer link = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(i)));
    hubDevices.Add(link.Get(0));
    spokeDevices.Add(link.Get(1));
  }

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer hubInterfaces = address.Assign(hubDevices);
  Ipv4InterfaceContainer spokeInterfaces = address.Assign(spokeDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 5000;
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(hubNode);
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  for (int i = 1; i < nNodes; ++i) {
    OnOffHelper onoff("ns3::TcpSocketFactory",
                      InetSocketAddress(hubInterfaces.GetAddress(i - 1), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("DataRate", StringValue(dataRate));
    ApplicationContainer onoffApp = onoff.Install(nodes.Get(i));
    onoffApp.Start(Seconds(2.0));
    onoffApp.Stop(Seconds(10.0));
  }

  p2pQueue.EnableAsciiAll("tcp-star-server.tr");

  for (int i = 0; i < hubDevices.GetN(); i++) {
      p2p.EnablePcap("tcp-star-server", hubDevices.Get(i), true);
  }

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}