#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpRelaySimulation");

class RelayApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("RelayApplication")
      .SetParent<Application>()
      .AddConstructor<RelayApplication>();
    return tid;
  }

  RelayApplication() {}
  virtual ~RelayApplication() {}

private:
  virtual void StartApplication(void)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 8080);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&RelayApplication::HandleRead, this));
  }

  virtual void StopApplication(void)
  {
    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      Ipv4Header ipHeader;
      packet->RemoveHeader(ipHeader);

      // Forward to server at 10.1.2.2:9
      InetSocketAddress dest = InetSocketAddress("10.1.2.2", 9);
      m_relaySocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      m_relaySocket->SendTo(packet, 0, dest);
      NS_LOG_INFO("Relayed packet to server");
    }
  }

  Ptr<Socket> m_socket;
  Ptr<Socket> m_relaySocket;
};

class ServerApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("ServerApplication")
      .SetParent<Application>()
      .AddConstructor<ServerApplication>();
    return tid;
  }

  ServerApplication() {}
  virtual ~ServerApplication() {}

private:
  virtual void StartApplication(void)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&ServerApplication::HandleRead, this));
  }

  virtual void StopApplication(void)
  {
    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_INFO("Server received packet of size " << packet->GetSize());
    }
  }

  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpRelaySimulation", LOG_LEVEL_INFO);

  NodeContainer clientNode, relayNode, serverNode;
  clientNode.Create(1);
  relayNode.Create(1);
  serverNode.Create(1);

  NodeContainer nodes1 = NodeContainer(clientNode, relayNode);
  NodeContainer nodes2 = NodeContainer(relayNode, serverNode);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices1 = p2p.Install(nodes1);
  NetDeviceContainer devices2 = p2p.Install(nodes2);

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Install applications
  RelayApplication relayApp;
  relayApp.SetNode(relayNode.Get(0));
  relayNode.Get(0)->AddApplication(&relayApp);
  relayApp.SetStartTime(Seconds(1.0));
  relayApp.SetStopTime(Seconds(10.0));

  ServerApplication serverApp;
  serverApp.SetNode(serverNode.Get(0));
  serverNode.Get(0)->AddApplication(&serverApp);
  serverApp.SetStartTime(Seconds(1.0));
  serverApp.SetStopTime(Seconds(10.0));

  // Client app sending UDP packets
  UdpClientHelper clientHelper(interfaces2.GetAddress(1), 8080);
  clientHelper.SetAttribute("MaxPackets", UintegerValue(5));
  clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = clientHelper.Install(clientNode);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}