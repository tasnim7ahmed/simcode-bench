#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceHostStaticRouting");

class MyPacketSink : public Application
{
public:
  static TypeId GetTypeId();
  MyPacketSink() {}
  virtual ~MyPacketSink() {}

private:
  virtual void StartApplication();
  virtual void StopApplication();
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
};

TypeId MyPacketSink::GetTypeId()
{
  static TypeId tid = TypeId("MyPacketSink")
    .SetParent<Application>()
    .AddConstructor<MyPacketSink>();
  return tid;
}

void MyPacketSink::StartApplication()
{
  if (m_socket == nullptr)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&MyPacketSink::HandleRead, this));
  }
}

void MyPacketSink::StopApplication()
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void MyPacketSink::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(65535, 0, from)))
  {
    if (InetSocketAddress::IsMatchingType(from))
    {
      NS_LOG_INFO("Received " << packet->GetSize() << " bytes from " <<
                  InetSocketAddress::ConvertFrom(from).GetIpv4() << ":" <<
                  InetSocketAddress::ConvertFrom(from).GetPort());
    }
  }
}

static void SourceRxCallback(Ptr<const Packet> packet, const Address &from)
{
  if (InetSocketAddress::IsMatchingType(from))
  {
    NS_LOG_INFO("Source node received packet from: " <<
                InetSocketAddress::ConvertFrom(from).GetIpv4());
  }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("MyPacketSink", LOG_LEVEL_INFO);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(4); // source, router1, router2, destination

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devSrcToR1 = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devSrcToR2 = p2p.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer devR1ToR2 = p2p.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer devR2ToDst = p2p.Install(nodes.Get(2), nodes.Get(3));

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifSrcToR1 = address.Assign(devSrcToR1);

  address.NewNetwork();
  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifSrcToR2 = address.Assign(devSrcToR2);

  address.NewNetwork();
  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ifR1ToR2 = address.Assign(devR1ToR2);

  address.NewNetwork();
  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer ifR2ToDst = address.Assign(devR2ToDst);

  Ipv4StaticRoutingHelper routingHelper;

  // Source to R1 route
  Ptr<Ipv4StaticRouting> srcStaticRouting = routingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
  srcStaticRouting->AddHostRouteTo(ifR2ToDst.GetAddress(1), ifSrcToR1.Get(0), 10); // higher metric
  srcStaticRouting->AddHostRouteTo(ifR2ToDst.GetAddress(1), ifSrcToR2.Get(0), 5); // lower metric preferred

  // Routers
  Ptr<Ipv4StaticRouting> r1StaticRouting = routingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
  r1StaticRouting->AddHostRouteTo(ifR2ToDst.GetAddress(1), ifR1ToR2.Get(1), 1);

  Ptr<Ipv4StaticRouting> r2StaticRouting = routingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
  r2StaticRouting->AddHostRouteTo(ifR2ToDst.GetAddress(1), ifR2ToDst.Get(0), 1);

  // Destination sink application
  MyPacketSink sinkApp;
  nodes.Get(3)->AddApplication(sinkApp.GetObject<Application>());
  sinkApp.SetStartTime(Seconds(1.0));
  sinkApp.SetStopTime(Seconds(10.0));

  // UDP Echo Client
  UdpEchoClientHelper echoClient(ifR2ToDst.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Socket binding to interface dynamically
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> sendSocket = Socket::CreateSocket(nodes.Get(0), tid);
  sendSocket->Bind();
  sendSocket->Connect(InetSocketAddress(ifR2ToDst.GetAddress(1), 9));
  sendSocket->SetAttribute("IpTtl", UintegerValue(255));
  sendSocket->SetRecvCallback(MakeCallback(&SourceRxCallback));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}