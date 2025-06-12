#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

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
                    Ipv4AddressValue(Ipv4Address::GetAny()),
                    MakeIpv4AddressAccessor(&UdpRelayApplication::m_local),
                    MakeIpv4AddressChecker())
      .AddAttribute("RemoteAddress", "The remote server address to forward packets to.",
                    Ipv4AddressValue(),
                    MakeIpv4AddressAccessor(&UdpRelayApplication::m_remoteAddress),
                    MakeIpv4AddressChecker())
      .AddAttribute("RemotePort", "The remote server port to forward packets to.",
                    UintegerValue(0),
                    MakeUintegerAccessor(&UdpRelayApplication::m_remotePort),
                    MakeUintegerChecker<uint16_t>());
    return tid;
  }

  UdpRelayApplication() : m_socket(nullptr) {}
  virtual ~UdpRelayApplication() { }

protected:
  void DoInitialize() override
  {
    Application::DoInitialize();
    if (m_socket == nullptr)
    {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
      InetSocketAddress local(m_local, m_serverPort);
      m_socket->Bind(local);
    }
    m_socket->SetRecvCallback(MakeCallback(&UdpRelayApplication::HandleRead, this));
  }

private:
  void StartApplication() override { }

  void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      if (!InetSocketAddress::IsMatchingType(from))
        continue;

      InetSocketAddress fromAddr = InetSocketAddress::ConvertFrom(from);
      NS_LOG_INFO("Relay received " << packet->GetSize() << " bytes from " << fromAddr.GetIpv4()
                                   << ":" << fromAddr.GetPort());

      // Forward packet to server
      InetSocketAddress dest(m_remoteAddress, m_remotePort);
      m_socket->SendTo(packet, 0, dest);
    }
  }

  Ipv4Address m_local;
  uint16_t m_serverPort = 9;
  Ipv4Address m_remoteAddress;
  uint16_t m_remotePort = 9;
  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpRelaySimulation", LOG_LEVEL_INFO);

  NodeContainer clientNode, relayNode, serverNode;
  clientNode.Create(1);
  relayNode.Create(1);
  serverNode.Create(1);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer clientRelayDevices = p2p.Install(clientNode, relayNode);
  NetDeviceContainer relayServerDevices = p2p.Install(relayNode, serverNode);

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer clientRelayInterfaces = address.Assign(clientRelayDevices);
  address.NewNetwork();
  Ipv4InterfaceContainer relayServerInterfaces = address.Assign(relayServerDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Server setup: use UDP echo server for logging
  UdpEchoServerHelper server(9);
  ApplicationContainer serverApp = server.Install(serverNode);
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Relay setup
  UdpRelayApplication relayApp;
  relayApp.SetAttribute("Local", Ipv4AddressValue(relayClientInterfaces.GetAddress(1)));
  relayApp.SetAttribute("RemoteAddress", Ipv4AddressValue(relayServerInterfaces.GetAddress(1)));
  relayApp.SetAttribute("RemotePort", UintegerValue(9));
  relayNode.Get(0)->AddApplication(&relayApp);
  relayApp.SetStartTime(Seconds(1.0));
  relayApp.SetStopTime(Seconds(10.0));

  // Client application
  UdpEchoClientHelper client(relayClientInterfaces.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(5));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp = client.Install(clientNode);
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}