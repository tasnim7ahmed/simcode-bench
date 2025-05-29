#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-sink.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpClientServerExample");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
  LogComponent::SetDefaultLogComponentEnable(true);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 5000;

  // Server (Node 1)
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  // Client (Node 0)
  BulkSendHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send unlimited data
  clientHelper.SetAttribute("SendSize", UintegerValue(1024)); // Packet size
  clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  clientHelper.SetAttribute("PacketRate", DataRateValue(DataRate("5Mbps")));

  ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  // PCAP Tracing
  pointToPoint.EnablePcapAll("tcp-client-server");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}