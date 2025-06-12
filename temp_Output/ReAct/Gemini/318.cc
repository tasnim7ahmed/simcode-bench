#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpExample");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 8080;
  Address sinkLocalAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  Ptr<Node> clientNode = nodes.Get(0);
  Ptr<Ipv4> ipv4 = clientNode->GetObject<Ipv4>();
  Ipv4Address serverAddress = interfaces.GetAddress(1);

  BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(serverAddress, port));
  bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(1024 * 10));
  bulkSendHelper.SetAttribute("SendSize", UintegerValue(1024));
  ApplicationContainer sourceApp = bulkSendHelper.Install(nodes.Get(0));
  sourceApp.Start(Seconds(2.0));
  sourceApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}