#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/tcp-socket.h"

using namespace ns3;

void
RttTracer(Ptr<Socket> socket)
{
  Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase>(socket);
  if (tcpSocket)
    {
      Time rtt = tcpSocket->GetRTT();
      std::cout << "At time " << Simulator::Now().GetSeconds() << "s, measured RTT: " << rtt.GetMilliSeconds() << " ms" << std::endl;
    }
  Simulator::Schedule(Seconds(0.1), &RttTracer, socket);
}

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 50000;

  Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
  clientHelper.SetAttribute("DataRate", StringValue("2Mbps"));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
  clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Ptr<Socket> tcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
  tcpSocket->Bind();
  RttTracer(tcpSocket);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}