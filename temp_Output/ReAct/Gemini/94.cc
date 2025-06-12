#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv6-header.h"
#include "ns3/udp-header.h"

#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6SocketOptionsExample");

class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp ();

  void Setup (Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate, Time interPacketInterval, uint8_t tclass, uint8_t hopLimit);
  void ChangeTclass (uint8_t newTclass);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void Tx (void);

  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  DataRate m_dataRate;
  Time m_interPacketInterval;
  Socket m_socket;
  uint32_t m_packetsSent;
  uint8_t m_tclass;
  uint8_t m_hopLimit;
};

MyApp::MyApp ()
  : m_socket (nullptr),
    m_packetsSent (0),
    m_tclass (0),
    m_hopLimit (0)
{
}

MyApp::~MyApp ()
{
  m_socket = nullptr;
}

void
MyApp::Setup (Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate,
               Time interPacketInterval, uint8_t tclass, uint8_t hopLimit)
{
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
  m_interPacketInterval = interPacketInterval;
  m_tclass = tclass;
  m_hopLimit = hopLimit;
}

void
MyApp::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), TypeId::LookupByName ("ns3::UdpSocketFactory"));
  m_socket->Bind ();
  m_socket->Connect (m_peer);

  if (m_tclass != 0)
    {
      m_socket->SetIpTos (m_tclass);
    }

  if (m_hopLimit != 0)
    {
      m_socket->SetTtl (m_hopLimit);
    }

  m_packetsSent = 0;
  ScheduleTx ();
}

void
MyApp::StopApplication (void)
{
  Simulator::Cancel (m_event);
  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::ScheduleTx (void)
{
  if (m_packetsSent < m_nPackets)
    {
      Time tNext (Seconds (m_interPacketInterval.GetSeconds()));
      m_event = Simulator::Schedule (tNext, &MyApp::Tx, this);
    }
}

void
MyApp::Tx (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (InetSocketAddress::IsMatchingType (m_peer))
    {
      InetSocketAddress iaddr = InetSocketAddress::ConvertFrom (m_peer);
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                               << "s packet " << m_packetsSent
                               << " sent to " << iaddr.GetIpv4 ()
                               << " port " << iaddr.GetPort ());
    }
  else if (Inet6SocketAddress::IsMatchingType (m_peer))
    {
      Inet6SocketAddress iaddr = Inet6SocketAddress::ConvertFrom (m_peer);
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                               << "s packet " << m_packetsSent
                               << " sent to " << iaddr.GetIpv6 ()
                               << " port " << iaddr.GetPort ());
    }
  else
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                               << "s packet " << m_packetsSent
                               << " sent");
    }
  m_packetsSent++;
  ScheduleTx ();
}

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nPackets = 1;
  Time interPacketInterval = Seconds (1.0);
  uint32_t packetSize = 1024;
  uint16_t port = 9;
  uint8_t tclass = 0;
  uint8_t hopLimit = 0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("nPackets", "Number of packets generated", nPackets);
  cmd.AddValue ("packetSize", "Size of packet generated", packetSize);
  cmd.AddValue ("interPacketInterval", "Interval between packets", interPacketInterval);
  cmd.AddValue ("port", "Port number", port);
  cmd.AddValue ("tclass", "Traffic Class value", tclass);
  cmd.AddValue ("hopLimit", "Hop Limit value", hopLimit);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("Ipv6SocketOptionsExample", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:0:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (devices);

  Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (0)->GetObject<Ipv6> ()->GetRoutingProtocol (0));
  staticRouting->SetDefaultRoute (Ipv6Address ("2001:db8:0:1::2"), 0);
  staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (1)->GetObject<Ipv6> ()->GetRoutingProtocol (0));
  staticRouting->SetDefaultRoute (Ipv6Address ("2001:db8:0:1::1"), 0);

  Address sinkAddress (Inet6SocketAddress (interfaces.GetAddress (1, 1), port));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  Ptr<MyApp> app = CreateObject<MyApp> ();
  app->Setup (sinkAddress, packetSize, nPackets, DataRate ("5Mbps"), interPacketInterval, tclass, hopLimit);
  nodes.Get (0)->AddApplication (app);
  app->SetStartTime (Seconds (1.0));
  app->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}