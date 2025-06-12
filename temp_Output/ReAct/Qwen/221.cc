#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpClientServerSimulation");

class CustomUdpServer : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("CustomUdpServer")
      .SetParent<Application> ()
      .AddConstructor<CustomUdpServer> ()
      .AddAttribute ("Port", "Listening port",
                     UintegerValue (9),
                     MakeUintegerAccessor (&CustomUdpServer::m_port),
                     MakeUintegerChecker<uint16_t> ());
    return tid;
  }

  CustomUdpServer () {}
  virtual ~CustomUdpServer () { m_socket = nullptr; }

private:
  void StartApplication (void)
  {
    if (!m_socket)
      {
        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket (GetNode (), tid);
        InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
        m_socket->Bind (local);
        m_socket->SetRecvCallback (MakeCallback (&CustomUdpServer::HandleRead, this));
      }
  }

  void StopApplication (void)
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      }
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        NS_LOG_INFO ("Server received packet of size " << packet->GetSize () << " from " << from);
      }
  }

  uint16_t m_port = 9;
  Ptr<Socket> m_socket;
};

class CustomUdpClient : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("CustomUdpClient")
      .SetParent<Application> ()
      .AddConstructor<CustomUdpClient> ()
      .AddAttribute ("RemoteAddress", "Destination IP address",
                     Ipv4AddressValue (),
                     MakeIpv4AddressAccessor (&CustomUdpClient::m_remoteAddress),
                     MakeIpv4AddressChecker ())
      .AddAttribute ("RemotePort", "Destination port",
                     UintegerValue (9),
                     MakeUintegerAccessor (&CustomUdpClient::m_remotePort),
                     MakeUintegerChecker<uint16_t> ())
      .AddAttribute ("Interval", "Time between packets (seconds)",
                     TimeValue (Seconds (1.0)),
                     MakeTimeAccessor (&CustomUdpClient::m_interval),
                     MakeTimeChecker ())
      .AddAttribute ("PacketSize", "Size of each packet",
                     UintegerValue (512),
                     MakeUintegerAccessor (&CustomUdpClient::m_packetSize),
                     MakeUintegerChecker<uint32_t> ());
    return tid;
  }

  CustomUdpClient () {}
  virtual ~CustomUdpClient () { m_socket = nullptr; }

private:
  void StartApplication (void)
  {
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket (GetNode (), tid);

    m_sendEvent = Simulator::Schedule (Seconds (0.0), &CustomUdpClient::SendPacket, this);
  }

  void StopApplication (void)
  {
    if (m_socket)
      {
        m_socket->Close ();
      }
    Simulator::Cancel (m_sendEvent);
  }

  void SendPacket (void)
  {
    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    m_socket->SendTo (packet, 0, InetSocketAddress (m_remoteAddress, m_remotePort));
    NS_LOG_INFO ("Client sent packet to " << m_remoteAddress << ":" << m_remotePort);
    m_sendEvent = Simulator::Schedule (m_interval, &CustomUdpClient::SendPacket, this);
  }

  Ipv4Address m_remoteAddress;
  uint16_t m_remotePort = 9;
  Time m_interval;
  uint32_t m_packetSize = 512;
  EventId m_sendEvent;
  Ptr<Socket> m_socket;
};

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (3); // client + 2 servers

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Install custom server applications on node 1 and node 2
  uint16_t serverPort1 = 4000;
  uint16_t serverPort2 = 5000;

  CustomUdpServerHelper server1 (serverPort1);
  ApplicationContainer serverApp1 = server1.Install (nodes.Get (1));
  serverApp1.Start (Seconds (0.0));
  serverApp1.Stop (Seconds (10.0));

  CustomUdpServerHelper server2 (serverPort2);
  ApplicationContainer serverApp2 = server2.Install (nodes.Get (2));
  serverApp2.Start (Seconds (0.0));
  serverApp2.Stop (Seconds (10.0));

  // Install custom client application on node 0
  CustomUdpClientHelper client1;
  client1.SetAttribute ("RemoteAddress", Ipv4AddressValue (interfaces.GetAddress (1)));
  client1.SetAttribute ("RemotePort", UintegerValue (serverPort1));
  client1.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client1.SetAttribute ("PacketSize", UintegerValue (1024));

  CustomUdpClientHelper client2;
  client2.SetAttribute ("RemoteAddress", Ipv4AddressValue (interfaces.GetAddress (2)));
  client2.SetAttribute ("RemotePort", UintegerValue (serverPort2));
  client2.SetAttribute ("Interval", TimeValue (Seconds (2.0)));
  client2.SetAttribute ("PacketSize", UintegerValue (512));

  ApplicationContainer clientApps;
  clientApps.Add (client1.Install (nodes.Get (0)));
  clientApps.Add (client2.Install (nodes.Get (0)));

  clientApps.Start (Seconds (0.0));
  clientApps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}