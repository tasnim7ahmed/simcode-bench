#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/nat-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NatSimulation");

class NatTestApp : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("NatTestApp")
                            .SetParent<Application>()
                            .AddConstructor<NatTestApp>();
    return tid;
  }

  NatTestApp();
  virtual ~NatTestApp();

protected:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

private:
  void SendPacket(void);
  Ptr<Socket> m_socket;
  Address m_serverAddr;
  EventId m_sendEvent;
};

NatTestApp::NatTestApp()
    : m_socket(0),
      m_sendEvent()
{
}

NatTestApp::~NatTestApp()
{
  m_socket = 0;
}

void NatTestApp::StartApplication()
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress serverAddr(Ipv4Address("198.10.1.10"), 9); // Public server IP
  m_serverAddr = serverAddr;
  Simulator::ScheduleNow(&NatTestApp::SendPacket, this);
}

void NatTestApp::StopApplication()
{
  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }

  if (m_socket)
  {
    m_socket->Close();
  }
}

void NatTestApp::SendPacket()
{
  std::string payload = "Hello from private network!";
  Ptr<Packet> packet = Create<Packet>((uint8_t *)payload.c_str(), payload.size());
  m_socket->SendTo(packet, 0, m_serverAddr);

  NS_LOG_INFO("Sent UDP packet from node " << GetNode()->GetId());

  m_sendEvent = Simulator::Schedule(Seconds(1.0), &NatTestApp::SendPacket, this);
}

static void
RcvdPacket(Ptr<const Packet> p, const Address &addr)
{
  NS_LOG_INFO("Received packet at server with size: " << p->GetSize());
}

int main(int argc, char *argv[])
{
  LogComponentEnable("NatSimulation", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer privateNodes;
  privateNodes.Create(2);

  Ptr<Node> natRouter = CreateObject<Node>();
  Ptr<Node> publicServer = CreateObject<Node>();

  NodeContainer allNodes(privateNodes, natRouter, publicServer);

  InternetStackHelper internet;
  internet.Install(allNodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer privateDevices;
  NetDeviceContainer routerPublicDevice;
  NetDeviceContainer serverPublicDevice;

  // Private network: nodes to NAT router
  for (auto i = 0; i < 2; ++i)
  {
    NodeContainer pair(privateNodes.Get(i), natRouter);
    NetDeviceContainer devices = p2p.Install(pair);
    privateDevices.Add(devices);
  }

  // Public network: NAT router to public server
  NodeContainer routerServer(natRouter, publicServer);
  routerPublicDevice = p2p.Install(routerServer.Get(0), routerServer.Get(1));
  serverPublicDevice.Add(routerPublicDevice.Get(1));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer privateInterfaces = ipv4.Assign(privateDevices);

  ipv4.SetBase("198.10.1.0", "255.255.255.0");
  Ipv4InterfaceContainer publicInterfaces = ipv4.Assign(routerPublicDevice);

  // Assign public IP to server
  ipv4.Assign(serverPublicDevice);
  publicInterfaces.Add(serverPublicDevice);

  // Set up NAT on the router
  NatHelper nat;
  nat.SetNatNetwork("198.10.1.0", "255.255.255.0");
  nat.Assign(natRouter, publicInterfaces.GetAddress(0));

  // Assign IP addresses to hosts
  Ipv4InterfaceContainer host1Interface;
  host1Interface.Add(privateInterfaces.Get(0), 0);
  Ipv4InterfaceContainer host2Interface;
  host2Interface.Add(privateInterfaces.Get(1), 0);

  // Install applications on private hosts
  ApplicationContainer apps;

  for (auto i = 0; i < 2; ++i)
  {
    Ptr<NatTestApp> app = CreateObject<NatTestApp>();
    privateNodes.Get(i)->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(5.0));
  }

  // Install echo server on public server
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(publicServer);
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoServer/Tx", MakeCallback(&RcvdPacket));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}