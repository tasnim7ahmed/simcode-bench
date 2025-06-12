#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TbfExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  std::string dataRate = "5Mbps";
  std::string delay = "2ms";
  uint32_t queueSize = 10;
  uint32_t mtu = 1500;
  std::string onOffRate = "4Mbps";
  double onTime = 1.0;
  double offTime = 1.0;
  double simTime = 10.0;
  uint32_t maxBytes = 0;
  uint32_t maxPackets = 0;
  std::string shapingRate = "6Mbps";
  std::string firstBucketSize = "10000";
  std::string secondBucketSize = "20000";
  bool enablePcap = false;

  cmd.AddValue ("dataRate", "The data rate for the point-to-point link", dataRate);
  cmd.AddValue ("delay", "The delay for the point-to-point link", delay);
  cmd.AddValue ("queueSize", "Max Packets allowed in the Queue", queueSize);
  cmd.AddValue ("mtu", "MTU for point-to-point links", mtu);
  cmd.AddValue ("onOffRate", "Rate of the OnOff application", onOffRate);
  cmd.AddValue ("onTime", "On Time of the OnOff application", onTime);
  cmd.AddValue ("offTime", "Off Time of the OnOff application", offTime);
  cmd.AddValue ("simTime", "The simulation time", simTime);
  cmd.AddValue ("maxBytes", "Max Bytes for the sink application", maxBytes);
  cmd.AddValue ("maxPackets", "Max Packets for the sink application", maxPackets);
  cmd.AddValue ("shapingRate", "The shaping rate for the TBF queue", shapingRate);
  cmd.AddValue ("firstBucketSize", "The size of the first bucket in the TBF queue", firstBucketSize);
  cmd.AddValue ("secondBucketSize", "The size of the second bucket in the TBF queue", secondBucketSize);
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);

  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (delay));
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  TrafficControlHelper tch;
  QueueDiscContainer qdiscs;
  TrafficControlLayer::TbfQdiscParams tbfParams;
  tbfParams.peakRate = shapingRate;
  tbfParams.sizeFirstBucket = firstBucketSize;
  tbfParams.sizeSecondBucket = secondBucketSize;
  tbfParams.enableTrace = true;

  qdiscs = tch.Install (devices.Get (0), tbfParams);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  ApplicationContainer sinkApp;
  uint16_t sinkPort = 9;
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), sinkPort));
  sinkApp = sinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime));

  OnOffHelper onOffHelper ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (1), sinkPort)));
  onOffHelper.SetConstantRate (DataRate (onOffRate));
  onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + std::to_string(onTime) + "]"));
  onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + std::to_string(offTime) + "]"));
  ApplicationContainer onOffApp = onOffHelper.Install (nodes.Get (0));
  onOffApp.Start (Seconds (1.0));
  onOffApp.Stop (Seconds (simTime));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  if (enablePcap)
  {
    pointToPoint.EnablePcapAll ("tbf-example");
  }

  Simulator::Stop (Seconds (simTime));

  Simulator::Run ();

  qdiscs.Get (0)->AggregateFlowMonitorResults(std::cout, std::cout);

  Simulator::Destroy ();
  return 0;
}