#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpClientServer");

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
  virtual ~CustomUdpServer () { }

protected:
  void DoStart (void) override
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
    m_socket->Bind (local);
    m_socket->SetRecvCallback (MakeCallback (&CustomUdpServer::HandleRead, this));
  }

private:
  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
    {
      NS_LOG_INFO ("Received packet of size " << packet->GetSize () << " from " << from);
    }
  }

  uint16_t m_port = 0;
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
      .AddAttribute ("Interval", "Time between packets",
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
  virtual ~CustomUdpClient () { }

protected:
  void DoStart (void) override
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    m_socket->Connect (InetSocketAddress (m_remoteAddress, m_remotePort));
    SendPacket ();
  }

  void SendPacket (void)
  {
    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    m_socket->Send (packet);
    NS_LOG_INFO ("Sent packet of size " << m_packetSize << " to " << m_remoteAddress << ":" << m_remotePort);
    Simulator::Schedule (m_interval, &CustomUdpClient::SendPacket, this);
  }

private:
  Ipv4Address m_remoteAddress;
  uint16_t m_remotePort = 0;
  Time m_interval;
  uint32_t m_packetSize = 0;
  Ptr<Socket> m_socket;
};

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  NodeContainer clientNode;
  NodeContainer serverNodes;
  clientNode.Create (1);
  serverNodes.Create (2);

  NodeContainer csmaSwitch;
  csmaSwitch.Create (1);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer clientDevices;
  clientDevices.Add (csma.Install (NodeContainer (clientNode, csmaSwitch)));

  NetDeviceContainer serverDevices;
  serverDevices.Add (csma.Install (NodeContainer (serverNodes.Get (0), csmaSwitch)));
  serverDevices.Add (csma.Install (NodeContainer (serverNodes.Get (1), csmaSwitch)));

  InternetStackHelper stack;
  stack.InstallAll ();

  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.255.0");

  Ipv4InterfaceContainer clientInterfaces;
  clientInterfaces.Add (address.Assign (clientDevices.Get (0)));

  Ipv4InterfaceContainer serverInterfaces;
  serverInterfaces.Add (address.Assign (serverDevices.Get (0)));
  serverInterfaces.Add (address.Assign (serverDevices.Get (1)));

  // Server 1 on port 8000
  CustomUdpServerHelper server1Helper;
  server1Helper.SetAttribute ("Port", UintegerValue (8000));
  ApplicationContainer serverApps1 = server1Helper.Install (serverNodes.Get (0));
  serverApps1.Start (Seconds (0.0));
  serverApps1.Stop (Seconds (10.0));

  // Server 2 on port 8001
  CustomUdpServerHelper server2Helper;
  server2Helper.SetAttribute ("Port", UintegerValue (8001));
  ApplicationContainer serverApps2 = server2Helper.Install (serverNodes.Get (1));
  serverApps2.Start (Seconds (0.0));
  serverApps2.Stop (Seconds (10.0));

  // Client sending to Server 1
  CustomUdpClientHelper client1Helper;
  client1Helper.SetAttribute ("RemoteAddress", Ipv4AddressValue (serverInterfaces.GetAddress (0)));
  client1Helper.SetAttribute ("RemotePort", UintegerValue (8000));
  client1Helper.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client1Helper.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer clientApps1 = client1Helper.Install (clientNode);
  clientApps1.Start (Seconds (1.0));
  clientApps1.Stop (Seconds (10.0));

  // Client sending to Server 2
  CustomUdpClientHelper client2Helper;
  client2Helper.SetAttribute ("RemoteAddress", Ipv4AddressValue (serverInterfaces.GetAddress (1)));
  client2Helper.SetAttribute ("RemotePort", UintegerValue (8001));
  client2Helper.SetAttribute ("Interval", TimeValue (Seconds (2.0)));
  client2Helper.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer clientApps2 = client2Helper.Install (clientNode);
  clientApps2.Start (Seconds (1.0));
  clientApps2.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}