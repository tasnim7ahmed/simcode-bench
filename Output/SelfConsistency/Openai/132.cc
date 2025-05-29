// two-node-dv-loop.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

// Custom application to send UDP packets to a specific destination
class UdpGenerator : public Application
{
public:
  UdpGenerator () {}
  virtual ~UdpGenerator () {}

  void Setup (Ipv4Address dst, uint16_t port, uint32_t packetSize, uint32_t nPackets, Time interval)
  {
    m_dstAddress = dst;
    m_dstPort = port;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_interval = interval;
  }

private:
  virtual void StartApplication () override
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    m_socket->Connect (InetSocketAddress (m_dstAddress, m_dstPort));
    m_sent = 0;
    SendPacket ();
  }

  virtual void StopApplication () override
  {
    if (m_socket)
      {
        m_socket->Close ();
      }
  }

  void SendPacket ()
  {
    if (m_sent < m_nPackets)
      {
        Ptr<Packet> packet = Create<Packet> (m_packetSize);
        m_socket->Send (packet);
        ++m_sent;
        Simulator::Schedule (m_interval, &UdpGenerator::SendPacket, this);
      }
  }

  Ptr<Socket> m_socket;
  Ipv4Address m_dstAddress;
  uint16_t m_dstPort;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  Time m_interval;
  uint32_t m_sent;
};

// Custom application to log every received packet (so we can see looping)
class UdpLogger : public Application
{
public:
  UdpLogger () {}
  virtual ~UdpLogger () {}

  void Setup (uint16_t port, std::string nodeName)
  {
    m_port = port;
    m_nodeName = nodeName;
  }

private:
  virtual void StartApplication () override
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
    m_socket->Bind (local);

    m_socket->SetRecvCallback (MakeCallback (&UdpLogger::HandleRead, this));
  }

  virtual void StopApplication () override
  {
    if (m_socket)
      {
        m_socket->Close ();
      }
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        InetSocketAddress addr = InetSocketAddress::ConvertFrom (from);
        NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s: Node "
                        << m_nodeName << " received " << packet->GetSize ()
                        << " bytes from " << addr.GetIpv4 ());
      }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
  std::string m_nodeName;
};

int main (int argc, char *argv[])
{
  // Log for output
  LogComponentEnable("UdpLogger", LOG_LEVEL_INFO);

  // Nodes: 0 = A, 1 = B, 2 = Dst
  NodeContainer nodes;
  nodes.Create (3);

  // Point-to-point links
  NodeContainer ab = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer adst = NodeContainer (nodes.Get (0), nodes.Get (2));
  NodeContainer bdst = NodeContainer (nodes.Get (1), nodes.Get (2));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install on all links
  NetDeviceContainer dev_ab = p2p.Install (ab);
  NetDeviceContainer dev_adst = p2p.Install (adst);
  NetDeviceContainer dev_bdst = p2p.Install (bdst);

  // Stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_ab = ipv4.Assign (dev_ab);

  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_adst = ipv4.Assign (dev_adst);

  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_bdst = ipv4.Assign (dev_bdst);

  // For simplicity, we use static routing to explicitly set up the "distance vector" state.
  // The failure event will simulate the "count to infinity" problem by deliberately creating a loop.

  // Initial state: Each node knows the right route to the destination (Node 2)
  Ipv4StaticRoutingHelper staticRouting;
  Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4Dst = nodes.Get(2)->GetObject<Ipv4> ();

  Ptr<Ipv4StaticRouting> staticA = staticRouting.GetStaticRouting (ipv4A);
  Ptr<Ipv4StaticRouting> staticB = staticRouting.GetStaticRouting (ipv4B);

  // A: via direct to dst
  staticA->AddNetworkRouteTo (
      Ipv4Address ("10.0.3.0"), Ipv4Mask ("255.255.255.0"),
      iface_adst.GetAddress (1), 1);

  // B: via direct to dst
  staticB->AddNetworkRouteTo (
      Ipv4Address ("10.0.2.0"), Ipv4Mask ("255.255.255.0"),
      iface_bdst.GetAddress (1), 2);

  // A and B can also reach each other (direct)
  staticA->AddNetworkRouteTo (
      Ipv4Address ("10.0.1.0"), Ipv4Mask ("255.255.255.0"),
      iface_ab.GetAddress (1), 0);
  staticB->AddNetworkRouteTo (
      Ipv4Address ("10.0.1.0"), Ipv4Mask ("255.255.255.0"),
      iface_ab.GetAddress (0), 1);

  // Application: send UDP from A (10.0.1.1) to Dst (10.0.2.2)
  uint16_t udpPort = 9000;
  Ptr<UdpGenerator> appGen = CreateObject<UdpGenerator> ();
  appGen->Setup (iface_adst.GetAddress (1), udpPort, 100, 50, Seconds (0.5));
  nodes.Get (0)->AddApplication (appGen);
  appGen->SetStartTime (Seconds (1.0));
  appGen->SetStopTime (Seconds (15.0));

  // Application: logging on B (to see looping)
  Ptr<UdpLogger> logB = CreateObject<UdpLogger> ();
  logB->Setup (udpPort, "B");
  nodes.Get (1)->AddApplication (logB);
  logB->SetStartTime (Seconds (0.0));
  logB->SetStopTime (Seconds (20.0));

  // Application: logging on A (to see looping)
  Ptr<UdpLogger> logA = CreateObject<UdpLogger> ();
  logA->Setup (udpPort, "A");
  nodes.Get (0)->AddApplication (logA);
  logA->SetStartTime (Seconds (0.0));
  logA->SetStopTime (Seconds (20.0));

  // Application: logging on destination
  Ptr<UdpLogger> logDst = CreateObject<UdpLogger> ();
  logDst->Setup (udpPort, "Dst");
  nodes.Get (2)->AddApplication (logDst);
  logDst->SetStartTime (Seconds (0.0));
  logDst->SetStopTime (Seconds (20.0));

  // At t=4s, simulate destination link failures (disconnect both A and B from dst)
  Simulator::Schedule (Seconds (4.0), [&] () {
    NS_LOG_UNCOND ("*** Simulating destination failure: Shutting down links to dst at t=4s");

    // Set link-down for interfaces to dst
    dev_adst.Get (0)->SetReceiveErrorModel (CreateObject<RateErrorModel>());
    dev_bdst.Get (0)->SetReceiveErrorModel (CreateObject<RateErrorModel>());
    dev_adst.Get (1)->SetReceiveErrorModel (CreateObject<RateErrorModel>());
    dev_bdst.Get (1)->SetReceiveErrorModel (CreateObject<RateErrorModel>());

    // Remove original dst routes to simulate the failure
    staticA->RemoveRoute (0); // remove direct dst route in A (to 10.0.3.0)
    staticB->RemoveRoute (0); // remove direct dst route in B (to 10.0.2.0)
    // Now, both A and B use their routes to each other, "thinking" the other knows a route to dst

    // Set up the loop: A points to B for dst, B points to A for dst
    staticA->AddNetworkRouteTo (
        Ipv4Address ("10.0.2.0"), Ipv4Mask ("255.255.255.0"),
        iface_ab.GetAddress (1), 0);
    staticB->AddNetworkRouteTo (
        Ipv4Address ("10.0.3.0"), Ipv4Mask ("255.255.255.0"),
        iface_ab.GetAddress (0), 1);

    // This creates the classic DV loop: A sends to B, B sends to A, forever.
  });

  Simulator::Stop (Seconds (18.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}