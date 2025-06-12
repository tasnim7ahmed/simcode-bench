#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpLargeTransfer");

static void
CwndTracer(std::string key, uint32_t oldValue, uint32_t newValue)
{
  std::cout << Simulator::Now().AsDouble() << " " << key << " " << oldValue << " " << newValue << std::endl;
}

int
main(int argc, char* argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  LogComponent::SetAttribute("TcpL4Protocol", "SocketType", StringValue("ns3::TcpNewReno"));

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(interfaces12.GetAddress(1), sinkPort));

  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
  ns3TcpSocket->Bind();
  ns3TcpSocket->Connect(sinkAddress);

  Config::ConnectWithoutContext(
      "/NodeList/0/ApplicationList/0/$ns3::TcpSocketBase/CongestionWindow", MakeCallback(&CwndTracer));

  uint32_t bytesTotal = 2000000;
  uint32_t bytesSent = 0;
  Time start = Seconds(2.0);
  Time interPacketInterval = MicroSeconds(1);

  Simulator::Schedule(start, [ns3TcpSocket, bytesTotal, &bytesSent, interPacketInterval]() {
    while (bytesSent < bytesTotal) {
      uint32_t bytesToSend = std::min(uint32_t(1448), bytesTotal - bytesSent);
      Ptr<Packet> packet = Create<Packet>(bytesToSend);
      ns3TcpSocket->Send(packet);
      bytesSent += bytesToSend;
      Simulator::Schedule(interPacketInterval, [ns3TcpSocket, bytesTotal, &bytesSent, interPacketInterval]() {
          });
    }
  });

  pointToPoint.EnableAsciiAll("tcp-large-transfer.tr");
  pointToPoint.EnablePcapAll("tcp-large-transfer");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}