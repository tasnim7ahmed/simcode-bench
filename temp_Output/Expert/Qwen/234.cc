#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-server.h"
#include "ns3/udp-client.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpRelaySimulation");

class UdpRelayApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpRelayApplication")
      .SetParent<Application>()
      .AddConstructor<UdpRelayApplication>()
      .AddAttribute("Local", "The address on which to bind the relay socket.",
                    AddressValue(),
                    MakeAddressAccessor(&UdpRelayApplication::m_local),
                    MakeAddressChecker())
      .AddAttribute("Remote", "The address to which the relay forwards packets.",
                    AddressValue(),
                    MakeAddressAccessor(&UdpRelayApplication::m_remote),
                    MakeAddressChecker());
    return tid;
  }

  UdpRelayApplication() {}
  virtual ~UdpRelayApplication() {}

private:
  void StartApplication(void)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    NS_ASSERT_MSG(m_socket->Bind(m_local) == 0, "Failed to bind relay socket");
    m_socket->SetRecvCallback(MakeCallback(&UdpRelayApplication::HandleRead, this));

    m_sendSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  }

  void StopApplication(void)
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
    if (m_sendSocket)
    {
      m_sendSocket->Close();
      m_sendSocket = nullptr;
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      m_sendSocket->SendTo(packet, 0, m_remote);
    }
  }

  Address m_local;
  Address m_remote;
  Ptr<Socket> m_socket;
  Ptr<Socket> m_sendSocket;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3); // client, relay, server

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices1 = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices2 = p2p.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

  address.NewNetwork();
  Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

  uint16_t port = 9;

  // Server application
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(nodes.Get(2));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Relay application
  NodeContainer relayNode = nodes.Get(1);
  Address relayLocal(interfaces1.GetAddress(1), port);
  Address serverAddr(interfaces2.GetAddress(1), port);

  UdpRelayApplication relayApp;
  relayApp.SetAttribute("Local", AddressValue(relayLocal));
  relayApp.SetAttribute("Remote", AddressValue(serverAddr));
  relayNode->AddApplication(relayApp.GetObject<Application>());
  relayApp.GetObject<Application>()->SetStartTime(Seconds(1.0));
  relayApp.GetObject<Application>()->SetStopTime(Seconds(10.0));

  // Client application
  UdpClientHelper client(interfaces2.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(5));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}