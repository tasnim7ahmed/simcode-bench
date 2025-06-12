#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpSocketExample");

class ServerApplication : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    TypeId tid = TypeId ("ServerApplication")
      .SetParent<Application> ()
      .SetGroupName ("Examples")
      .AddConstructor<ServerApplication> ();
    return tid;
  }

  ServerApplication () {}
  virtual ~ServerApplication () {}

private:
  virtual void StartApplication (void)
  {
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket (GetNode (), tid);
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 9);
    m_socket->Bind (local);
    m_socket->SetRecvCallback (MakeCallback (&ServerApplication::ReceivePacket, this));
  }

  virtual void StopApplication (void)
  {
    if (m_socket)
      {
        m_socket->Close ();
      }
  }

  void ReceivePacket (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        NS_LOG_INFO ("Received packet of size " << packet->GetSize () << " from " << from);
      }
  }

  Ptr<Socket> m_socket;
};

class ClientApplication : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    TypeId tid = TypeId ("ClientApplication")
      .SetParent<Application> ()
      .SetGroupName ("Examples")
      .AddConstructor<ClientApplication> ()
      .AddAttribute ("RemoteAddress", "The destination address of the UDP packets.",
                     Ipv4AddressValue (),
                     MakeIpv4AddressAccessor (&ClientApplication::m_remoteAddress),
                     MakeIpv4AddressChecker ())
      .AddAttribute ("Interval", "The time between two packets",
                     TimeValue (Seconds (1.0)),
                     MakeTimeAccessor (&ClientApplication::m_interval),
                     MakeTimeChecker ());
    return tid;
  }

  ClientApplication () {}
  virtual ~ClientApplication () {}

private:
  virtual void StartApplication (void)
  {
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket (GetNode (), tid);
    m_sendEvent = Simulator::ScheduleNow (&ClientApplication::SendPacket, this);
  }

  virtual void StopApplication (void)
  {
    Simulator::Cancel (m_sendEvent);
    if (m_socket)
      {
        m_socket->Close ();
      }
  }

  void SendPacket ()
  {
    Ptr<Packet> packet = Create<Packet> (1024); // 1024-byte packet
    m_socket->SendTo (packet, 0, InetSocketAddress (m_remoteAddress, 9));
    NS_LOG_INFO ("Sent packet of size 1024 to " << m_remoteAddress);
    m_sendEvent = Simulator::Schedule (m_interval, &ClientApplication::SendPacket, this);
  }

  Ptr<Socket> m_socket;
  Ipv4Address m_remoteAddress;
  Time m_interval;
  EventId m_sendEvent;
};

int main (int argc, char *argv[])
{
  LogComponentEnable ("CustomUdpSocketExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  ApplicationContainer serverApp;
  ServerApplication server;
  server.SetAttribute ("Protocol", TypeId::LookupByName ("ns3::UdpSocketFactory"));
  serverApp.Add (server.Install (nodes.Get (1)));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  ApplicationContainer clientApp;
  ClientApplication client;
  client.SetAttribute ("RemoteAddress", Ipv4Value (interfaces.GetAddress (1)));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  clientApp.Add (client.Install (nodes.Get (0)));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}