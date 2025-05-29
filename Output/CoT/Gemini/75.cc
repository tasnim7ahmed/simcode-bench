#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/queue.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(7);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer p2pDevices02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer p2pDevices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer p2pDevices56 = pointToPoint.Install(nodes.Get(5), nodes.Get(6));

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("1Mbps"));
  csma.SetChannelAttribute("Delay", StringValue("5ms"));

  NetDeviceContainer csmaDevices = csma.Install(NodeContainer(nodes.Get(2), nodes.Get(3), nodes.Get(4), nodes.Get(5)));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces02 = address.Assign(p2pDevices02);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces12 = address.Assign(p2pDevices12);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces56 = address.Assign(p2pDevices56);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  UdpServerHelper server(port);
  server.SetAttribute("Protocol", EnumValue(UdpServerHelper::ECHO));
  ApplicationContainer sinkApp = server.Install(nodes.Get(6));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  uint32_t packetSize = 210;
  DataRate dataRate("448Kbps");

  UdpClientHelper client(p2pInterfaces56.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000000));
  client.SetAttribute("Interval", TimeValue(Seconds(packetSize * 8 / dataRate.GetBitRate())));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll(ascii.CreateFileStream("mixed-network.tr"));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}