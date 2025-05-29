#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/tcp-client-socket-factory.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 8080;

  Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));

  TcpClientHelper clientHelper;
  clientHelper.SetAttribute("RemoteAddress", AddressValue(serverAddress));
  clientHelper.SetAttribute("RemotePort", UintegerValue(port));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
  clientHelper.SetAttribute("MaxPackets", UintegerValue(100));
  clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.1)));

  ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}