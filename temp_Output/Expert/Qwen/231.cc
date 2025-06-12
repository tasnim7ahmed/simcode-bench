#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPointToPointExample");

class ServerLoggingApp : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    return TypeId("ServerLoggingApp")
      .SetParent<Application>()
      .AddConstructor<ServerLoggingApp>();
  }

  ServerLoggingApp() {}
  virtual ~ServerLoggingApp() {}

private:
  virtual void StartApplication()
  {
    TypeId tid = TcpSocketFactory::GetTypeId();
    Ptr<Socket> socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 50000);
    socket->Bind(local);
    socket->Listen();
    socket->SetAcceptCallback(MakeCallback(&ServerLoggingApp::HandleAccept, this), MakeNullCallback<Ptr<Socket>, const Address&>());
  }

  virtual void StopApplication()
  {
    for (auto it = m_sockets.begin(); it != m_sockets.end(); ++it)
    {
      (*it)->Close();
    }
    m_sockets.clear();
  }

  void HandleAccept(Ptr<Socket> s, const Address&)
  {
    NS_LOG_INFO("Accepted connection");
    s->SetRecvCallback(MakeCallback(&ServerLoggingApp::HandleRead, this));
    m_sockets.push_back(s);
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    while ((packet = socket->Recv()))
    {
      NS_LOG_INFO("Received packet of size " << packet->GetSize());
    }
  }

  std::list<Ptr<Socket>> m_sockets;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  ApplicationContainer serverApps;
  serverApps.Add(CreateObject<ServerLoggingApp>());
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  TypeId tid = TcpSocketFactory::GetTypeId();
  Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), tid);
  InetSocketAddress remote(interfaces.GetAddress(1), 50000);
  clientSocket->Connect(remote);

  Simulator::Schedule(Seconds(1.0), &Socket::Send, clientSocket, Create<Packet>(1024), 0);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}