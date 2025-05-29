#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Configure Point-to-Point link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Install devices and channels
  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Configure TCP server (PacketSink) on node 1 (index 1) at port 5000
  uint16_t tcpPort = 5000;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), tcpPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  // Configure TCP client (OnOffApplication) on node 0 (index 0)
  OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));
  onoff.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
  onoff.SetAttribute("StopTime", TimeValue(Seconds(10.0)));

  ApplicationContainer clientApp = onoff.Install(nodes.Get(0));

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}