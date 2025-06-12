#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  uint32_t numNodes = 9;
  uint32_t packetSize = 1024;
  std::string dataRate = "5Mbps";

  CommandLine cmd;
  cmd.AddValue("numNodes", "Number of nodes", numNodes);
  cmd.AddValue("packetSize", "Size of packets", packetSize);
  cmd.AddValue("dataRate", "Data rate of CBR traffic", dataRate);
  cmd.Parse(argc, argv);

  LogComponentEnable("TcpStar", LOG_LEVEL_INFO);

  NodeContainer hubNode;
  hubNode.Create(1);

  NodeContainer spokeNodes;
  spokeNodes.Create(numNodes - 1);

  NodeContainer allNodes;
  allNodes.Add(hubNode);
  allNodes.Add(spokeNodes);

  InternetStackHelper stack;
  stack.Install(allNodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer hubDevices;
  NetDeviceContainer spokeDevices;

  for (uint32_t i = 0; i < numNodes - 1; ++i) {
    NetDeviceContainer devices = pointToPoint.Install(NodeContainer(hubNode.Get(0), spokeNodes.Get(i)));
    hubDevices.Add(devices.Get(0));
    spokeDevices.Add(devices.Get(1));
  }

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer hubInterfaces = address.Assign(hubDevices);
  Ipv4InterfaceContainer spokeInterfaces = address.Assign(spokeDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);

  ApplicationContainer sinkApp = sinkHelper.Install(hubNode.Get(0));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  for (uint32_t i = 0; i < numNodes - 1; ++i) {
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(hubInterfaces.GetAddress(i), port));
    source.SetAttribute("MaxBytes", UintegerValue(0));
    source.SetAttribute("SendSize", UintegerValue(packetSize));

    ApplicationContainer sourceApp = source.Install(spokeNodes.Get(i));
    sourceApp.Start(Seconds(2.0));
    sourceApp.Stop(Seconds(9.0));
  }

  for (uint32_t i = 0; i < numNodes - 1; ++i) {
    pointToPoint.EnablePcap("tcp-star-server", hubDevices.Get(i), true);
  }

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll(ascii.CreateFileStream("tcp-star-server.tr"));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}