#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-server.h"
#include "ns3/udp-client.h"

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

  RelayApplication()
      : m_socket(nullptr), m_relaySocket(nullptr) {}

  virtual ~RelayApplication() {}

protected:
  void DoInitialize() override
  {
    Application::DoInitialize();
  }

  void DoDispose() override
  {
    m_socket = nullptr;
    m_relaySocket = nullptr;
    Application::DoDispose();
  }

private:
  void StartApplication() override
  {
    if (!m_socket)
    {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
      m_socket->Bind(local);
      m_socket->SetRecvCallback(MakeCallback(&RelayApplication::HandleRead, this));
    }
  }

  void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_INFO("Relay received packet of size " << packet->GetSize());

      if (!m_relaySocket)
      {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_relaySocket = Socket::CreateSocket(GetNode(), tid);
      }

      // Forward to server at 10.1.2.2 port 9
      m_relaySocket->SendTo(packet, 0, InetSocketAddress("10.1.2.2", 9));
      NS_LOG_INFO("Relay forwarded packet to server");
    }
  }

  Ptr<Socket> m_socket;
  Ptr<Socket> m_relaySocket;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpRelaySimulation", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(3); // 0: client, 1: relay, 2: server

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices1, devices2;

  // Client <-> Relay
  devices1 = p2p.Install(nodes.Get(0), nodes.Get(1));

  // Relay <-> Server
  devices2 = p2p.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces1, interfaces2;

  address.SetBase("10.1.1.0", "255.255.255.0");
  interfaces1 = address.Assign(devices1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  interfaces2 = address.Assign(devices2);

  UdpServerHelper server(9);
  ApplicationContainer serverApp = server.Install(nodes.Get(2));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces2.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(5));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  RelayApplication relayApp;
  nodes.Get(1)->AddApplication(&relayApp);
  relayApp.SetStartTime(Seconds(1.0));
  relayApp.SetStopTime(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}