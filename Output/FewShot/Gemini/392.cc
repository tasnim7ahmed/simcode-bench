#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-base.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 5000;
  Address sinkLocalAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
  ns3TcpSocket->SetAttribute ("CongestionControl", StringValue ("ns3::TcpNewReno"));

  Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));

  TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
  Ptr<Socket> socket = Socket::CreateSocket(nodes.Get(0), tid);

  socket->Connect(serverAddress);

  uint32_t packetSize = 1024;
  uint32_t numPackets = 1000;

  BulkSendHelper source("ns3::TcpSocketFactory", serverAddress);
  source.SetAttribute("SendSize", UintegerValue(packetSize));
  source.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));
  ApplicationContainer sourceApp = source.Install(nodes.Get(0));
  sourceApp.Start(Seconds(2.0));
  sourceApp.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}