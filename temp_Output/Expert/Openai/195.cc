#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBulkTransferExample");

static uint64_t lastTotalRx = 0;

void
ThroughputMonitor(Ptr<Application> app, Ptr<PacketSink> sink, Time interval)
{
  uint64_t totalRx = sink->GetTotalRx();
  double throughput = (totalRx - lastTotalRx) * 8.0 / (interval.GetSeconds()) / 1e6; // Mbps
  Simulator::Now().GetSeconds();
  std::cout << Simulator::Now().GetSeconds()
            << "s: " << "Bytes received: " << totalRx
            << ", Throughput: " << throughput << " Mbps" << std::endl;
  lastTotalRx = totalRx;
  Simulator::Schedule(interval, &ThroughputMonitor, app, sink, interval);
}

int
main(int argc, char *argv[])
{
  std::string dataRate = "10Mbps";
  std::string delay = "5ms";
  uint32_t maxBytes = 0; // 0 means unlimited
  CommandLine cmd;
  cmd.AddValue("dataRate", "Data Rate", dataRate);
  cmd.AddValue("delay", "Link Delay", delay);
  cmd.AddValue("maxBytes", "Maximum bytes the application can send", maxBytes);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

  NetDeviceContainer devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 50000;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(20.0));

  BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
  bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
  ApplicationContainer sourceApps = bulkSendHelper.Install(nodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(20.0));

  pointToPoint.EnablePcapAll("tcp-bulk-transfer");

  lastTotalRx = 0;
  Simulator::Schedule(Seconds(1.1), &ThroughputMonitor, sourceApps.Get(0), DynamicCast<PacketSink>(sinkApps.Get(0)), Seconds(1.0));

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}