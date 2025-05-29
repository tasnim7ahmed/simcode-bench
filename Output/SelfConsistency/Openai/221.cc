#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpMultiServerSimulation");

// Custom UDP Server Application
class UdpServerCustom : public Application
{
public:
  UdpServerCustom () {}
  virtual ~UdpServerCustom () {}

  void Setup (uint16_t port)
  {
    m_port = port;
  }

private:
  virtual void StartApplication (void)
  {
    if (!m_socket)
      {
        m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
        InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
        m_socket->Bind (local);
        m_socket->SetRecvCallback (MakeCallback (&UdpServerCustom::HandleRead, this));
      }
    NS_LOG_INFO ("Server started on port " << m_port);
  }

  virtual void StopApplication (void)
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket = nullptr;
      }
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Address from;
    Ptr<Packet> packet;
    while ((packet = socket->RecvFrom (from)))
      {
        InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
        NS_LOG_UNCOND ("Time " << Simulator::Now ().GetSeconds ()
            << "s: Server (port " << m_port << ") received " << packet->GetSize ()
            << " bytes from " << address.GetIpv4 () << ":" << address.GetPort ());
      }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

// Custom UDP Client Application
class UdpClientCustom : public Application
{
public:
  UdpClientCustom () {}
  virtual ~UdpClientCustom () {}

  void Setup (std::vector<Ipv4Address> serverAddresses, std::vector<uint16_t> serverPorts,
              uint32_t packetSize, Time interval)
  {
    m_serverAddresses = serverAddresses;
    m_serverPorts = serverPorts;
    m_packetSize = packetSize;
    m_interval = interval;
    m_sendEvent = EventId ();
    m_socket = nullptr;
    m_packetCount = 0;
  }

private:
  virtual void StartApplication (void)
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());

    ScheduleSend ();
  }

  virtual void StopApplication (void)
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket = nullptr;
      }
    Simulator::Cancel (m_sendEvent);
  }

  void ScheduleSend ()
  {
    m_sendEvent = Simulator::Schedule (m_interval, &UdpClientCustom::SendPacket, this);
  }

  void SendPacket ()
  {
    for (size_t i = 0; i < m_serverAddresses.size (); ++i)
      {
        Ptr<Packet> packet = Create<Packet> (m_packetSize);
        InetSocketAddress dest = InetSocketAddress (m_serverAddresses[i], m_serverPorts[i]);
        m_socket->SendTo (packet, 0, dest);
        NS_LOG_UNCOND ("Time " << Simulator::Now ().GetSeconds ()
            << "s: Client sent " << m_packetSize << " bytes to "
            << m_serverAddresses[i] << ":" << m_serverPorts[i]);
      }
    ++m_packetCount;
    ScheduleSend ();
  }

  Ptr<Socket> m_socket;
  std::vector<Ipv4Address> m_serverAddresses;
  std::vector<uint16_t> m_serverPorts;
  uint32_t m_packetSize;
  Time m_interval;
  uint32_t m_packetCount;
  EventId m_sendEvent;
};

int main (int argc, char *argv[])
{
  LogComponentEnable("UdpMultiServerSimulation", LOG_LEVEL_INFO);

  // Network topology: client --csma-- switch --csma-- server1
  //                                   |--csma-- server2

  NodeContainer csmaNodes;
  csmaNodes.Create (3); // Node 0 = client, Node 1 = server1, Node 2 = server2

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = csma.Install (csmaNodes);

  InternetStackHelper stack;
  stack.Install (csmaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Server parameters
  uint16_t server1Port = 8000;
  uint16_t server2Port = 9000;
  Ptr<UdpServerCustom> serverApp1 = CreateObject<UdpServerCustom> ();
  serverApp1->Setup (server1Port);

  Ptr<UdpServerCustom> serverApp2 = CreateObject<UdpServerCustom> ();
  serverApp2->Setup (server2Port);

  csmaNodes.Get (1)->AddApplication (serverApp1);
  serverApp1->SetStartTime (Seconds (0.0));
  serverApp1->SetStopTime (Seconds (10.0));

  csmaNodes.Get (2)->AddApplication (serverApp2);
  serverApp2->SetStartTime (Seconds (0.0));
  serverApp2->SetStopTime (Seconds (10.0));

  // Client parameters
  std::vector<Ipv4Address> serverAddrs;
  serverAddrs.push_back (interfaces.GetAddress (1)); // Server 1
  serverAddrs.push_back (interfaces.GetAddress (2)); // Server 2

  std::vector<uint16_t> serverPorts;
  serverPorts.push_back (server1Port);
  serverPorts.push_back (server2Port);

  uint32_t packetSize = 1024;
  Time interPacketInterval = Seconds (1.0);

  Ptr<UdpClientCustom> clientApp = CreateObject<UdpClientCustom> ();
  clientApp->Setup (serverAddrs, serverPorts, packetSize, interPacketInterval);

  csmaNodes.Get (0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (1.0));
  clientApp->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}