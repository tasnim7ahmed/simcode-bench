#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceHostStaticRouting");

class TcpSender : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("TcpSender")
      .SetParent<Application>()
      .AddConstructor<TcpSender>()
      .AddAttribute("RemoteAddress", "The destination Address",
                    Ipv4AddressValue(),
                    MakeIpv4AddressAccessor(&TcpSender::m_destAddress),
                    MakeIpv4AddressChecker())
      .AddAttribute("RemotePort", "The destination port",
                    UintegerValue(50000),
                    MakeUintegerAccessor(&TcpSender::m_destPort),
                    MakeUintegerChecker<uint16_t>());
    return tid;
  }

  TcpSender() {}
  virtual ~TcpSender() {}

private:
  virtual void StartApplication(void)
  {
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    Ptr<Socket> sock = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress dest(m_destAddress, m_destPort);
    sock->Connect(dest);
    sock->Send(Create<Packet>(1024));
    sock->Close();
  }

  virtual void StopApplication(void) {}

  Ipv4Address m_destAddress;
  uint16_t m_destPort;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("TcpSender", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(5); // source (0), Rtr1 (1), Rtr2 (2), DestRtr (3), destination (4)

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1)); // source -> Rtr1
  NetDeviceContainer devices02 = p2p.Install(nodes.Get(0), nodes.Get(2)); // source -> Rtr2
  NetDeviceContainer devices13 = p2p.Install(nodes.Get(1), nodes.Get(3)); // Rtr1 -> DestRtr
  NetDeviceContainer devices23 = p2p.Install(nodes.Get(2), nodes.Get(3)); // Rtr2 -> DestRtr
  NetDeviceContainer devices34 = p2p.Install(nodes.Get(3), nodes.Get(4)); // DestRtr -> destination

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces01 = address.Assign(devices01);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces02 = address.Assign(devices02);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces13 = address.Assign(devices13);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces23 = address.Assign(devices23);

  address.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces34 = address.Assign(devices34);

  // Static routing configuration
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Add explicit static routes for source node
  Ptr<Ipv4> ipv4Src = nodes.Get(0)->GetObject<Ipv4>();
  uint32_t srcIf1 = ipv4Src->GetInterfaceForDevice(devices01.Get(0));
  uint32_t srcIf2 = ipv4Src->GetInterfaceForDevice(devices02.Get(0));

  // Route to destination through Rtr1
  Ipv4StaticRouting *routingSrc1 = Ipv4StaticRouting::GetRouting(ipv4Src);
  routingSrc1->AddHostRouteTo(Ipv4Address("10.1.5.2"), Ipv4Address("10.1.1.2"), srcIf1);

  // Route to destination through Rtr2
  Ipv4StaticRouting *routingSrc2 = Ipv4StaticRouting::GetRouting(ipv4Src);
  routingSrc2->AddHostRouteTo(Ipv4Address("10.1.5.2"), Ipv4Address("10.1.2.2"), srcIf2);

  // Server application on destination node
  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 50000));
  ApplicationContainer sinkApp = sink.Install(nodes.Get(4));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  // First transmission: use path via Rtr1
  Config::Set("/NodeList/0/ApplicationList/0/$TcpSender/RemoteAddress", Ipv4AddressValue("10.1.5.2"));
  ApplicationContainer senderApp1 = ApplicationContainer();
  Ptr<TcpSender> app1 = CreateObject<TcpSender>();
  nodes.Get(0)->AddApplication(app1);
  app1->SetStartTime(Seconds(1.0));
  app1->SetStopTime(Seconds(2.0));

  // Second transmission: use path via Rtr2
  // Change routing table temporarily or create new socket with specific interface
  // Alternatively, we can bind the socket to a specific interface
  // Here, we'll demonstrate by creating another sender using same route but different logic could be implemented

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}