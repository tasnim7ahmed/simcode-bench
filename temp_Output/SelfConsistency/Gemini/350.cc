#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpExample");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetAttribute("TcpL4Protocol", "SocketType", StringValue("ns3::TcpSocketFactory"));

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

  uint16_t port = 50000;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(0), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(0));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(1), TcpSocketFactory::GetTypeId());

  Ptr<BulkSendApplication> bulkSendApp = CreateObject<BulkSendApplication>();
  bulkSendApp->SetRemoteAddress(InetSocketAddress(interfaces.GetAddress(0), port));
  bulkSendApp->SetSocket(ns3TcpSocket);
  bulkSendApp->SetTotalBytes(10 * 1024); // Send 10 packets of 1024 bytes each
  bulkSendApp->SetSendSize(1024); // Set the packet size
  bulkSendApp->SetStartTime(Seconds(2.0));
  bulkSendApp->SetStopTime(Seconds(10.0));

  nodes.Get(1)->AddApplication(bulkSendApp);

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}