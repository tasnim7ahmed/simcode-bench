/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("StarTcpSimulation");

static uint32_t g_totalRx = 0;
static uint32_t g_totalTx = 0;
static uint32_t g_packetsLost = 0;
static std::vector<double> g_latencies;

void
RxPacket (Ptr<const Packet> packet, const Address &from)
{
  g_totalRx += packet->GetSize ();
}

void
TxPacket (Ptr<const Packet> packet)
{
  g_totalTx += packet->GetSize ();
}

void
PacketDrop (Ptr<const Packet> packet)
{
  g_packetsLost += 1;
}

void
RecordLatency (uint32_t seq, Time sendTime)
{
  double latency = (Simulator::Now () - sendTime).GetMilliSeconds ();
  g_latencies.push_back (latency);
}

class LatencyTag : public Tag
{
public:
  LatencyTag () : m_sendTime (Seconds (0)) {}
  LatencyTag (Time sendTime) : m_sendTime (sendTime) {}

  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("LatencyTag")
      .SetParent<Tag> ()
      .AddConstructor<LatencyTag> ();
    return tid;
  }

  virtual TypeId GetInstanceTypeId () const
  {
    return GetTypeId ();
  }

  virtual void Serialize (TagBuffer i) const
  {
    int64_t t = m_sendTime.GetNanoSeconds ();
    i.Write ((const uint8_t *)&t, sizeof (int64_t));
  }

  virtual void Deserialize (TagBuffer i)
  {
    int64_t t;
    i.Read ((uint8_t *)&t, sizeof (int64_t));
    m_sendTime = NanoSeconds (t);
  }

  virtual uint32_t GetSerializedSize () const
  {
    return sizeof (int64_t);
  }

  virtual void Print (std::ostream &os) const
  {
    os << "LatencyTag sendTime=" << m_sendTime;
  }

  void SetSendTime (Time t)
  {
    m_sendTime = t;
  }

  Time GetSendTime () const
  {
    return m_sendTime;
  }

private:
  Time m_sendTime;
};

class CustomApp : public Application
{
public:
  CustomApp ();
  virtual ~CustomApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize,
                            uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  uint32_t        m_packetsSent;
  EventId         m_sendEvent;
};

CustomApp::CustomApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_packetsSent (0)
{
}

CustomApp::~CustomApp()
{
  m_socket = 0;
}

void
CustomApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize,
                  uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
CustomApp::StartApplication (void)
{
  m_packetsSent = 0;
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
CustomApp::StopApplication (void)
{
  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }
  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
CustomApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);

  // Add send-time tag
  LatencyTag tag (Simulator::Now ());
  packet->AddPacketTag (tag);

  m_socket->Send (packet);

  // For throughput
  g_totalTx += m_packetSize;

  ++m_packetsSent;

  if (m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
CustomApp::ScheduleTx (void)
{
  Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
  m_sendEvent = Simulator::Schedule (tNext, &CustomApp::SendPacket, this);
}

static void
ServerRx (Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom (from))
    {
      g_totalRx += packet->GetSize ();
      // Latency calculation
      LatencyTag tag;
      if (packet->PeekPacketTag (tag))
        {
          double latency = (Simulator::Now () - tag.GetSendTime ()).GetMilliSeconds ();
          g_latencies.push_back (latency);
        }
    }
}

int
main (int argc, char *argv[])
{
  uint32_t nClients = 5;
  uint32_t packetSize = 1024; // bytes
  uint32_t numPackets = 50;
  std::string dataRate = "500Kbps"; // per client
  std::string p2pDelay = "2ms";
  bool enableTracing = false;

  CommandLine cmd;
  cmd.AddValue ("nClients", "Number of client nodes", nClients);
  cmd.AddValue ("packetSize", "Size of each packet (bytes)", packetSize);
  cmd.AddValue ("numPackets", "Number of packets per client", numPackets);
  cmd.AddValue ("dataRate", "Data rate for client transmit (e.g., 500Kbps)", dataRate);
  cmd.AddValue ("delay", "Point-to-point link delay", p2pDelay);
  cmd.AddValue ("enableTracing", "Enable ascii and pcap tracing", enableTracing);
  cmd.Parse (argc, argv);

  NodeContainer serverNode;
  serverNode.Create (1);

  NodeContainer clientNodes;
  clientNodes.Create (nClients);

  // Topology: star, server at center
  NodeContainer allNodes;
  allNodes.Add (serverNode.Get (0));
  for (uint32_t i = 0; i < nClients; ++i)
    {
      allNodes.Add (clientNodes.Get (i));
    }

  // Point-to-point links: client <-> server
  std::vector<NodeContainer> p2pNodes (nClients);
  for (uint32_t i = 0; i < nClients; ++i)
    {
      p2pNodes[i] = NodeContainer (clientNodes.Get (i), serverNode.Get (0));
    }

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  p2p.SetChannelAttribute ("Delay", StringValue (p2pDelay));

  std::vector<NetDeviceContainer> devices (nClients);
  for (uint32_t i = 0; i < nClients; ++i)
    {
      devices[i] = p2p.Install (p2pNodes[i]);
    }

  InternetStackHelper internet;
  internet.Install (allNodes);

  // IP address assignment
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces (nClients);
  for (uint32_t i = 0; i < nClients; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i + 1 << ".0";
      address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
      interfaces[i] = address.Assign (devices[i]);
    }

  // Install server application
  uint16_t port = 9000;
  Address serverAddress (InetSocketAddress (interfaces[0].GetAddress (1), port)); // same server for all

  // Create one TCP sink socket on server, listen on port
  Ptr<Socket> serverSocket = Socket::CreateSocket (serverNode.Get (0), TcpSocketFactory::GetTypeId ());
  InetSocketAddress localAddress = InetSocketAddress (Ipv4Address::GetAny (), port);
  serverSocket->Bind (localAddress);
  serverSocket->Listen ();
  serverSocket->SetRecvCallback (MakeCallback (&ServerRx));

  // Install client apps
  std::vector<Ptr<CustomApp>> clientApps (nClients);
  ApplicationContainer appContainer;
  for (uint32_t i = 0; i < nClients; ++i)
    {
      Ptr<Socket> clientSocket = Socket::CreateSocket (clientNodes.Get (i), TcpSocketFactory::GetTypeId ());
      Address serverIfAddr = InetSocketAddress (interfaces[i].GetAddress (1), port);

      Ptr<CustomApp> app = CreateObject<CustomApp> ();
      app->Setup (clientSocket, serverIfAddr, packetSize, numPackets, DataRate (dataRate));
      clientNodes.Get (i)->AddApplication (app);

      app->SetStartTime (Seconds (1.0 + i * 0.1)); // Stagger to avoid TCP SYN collisions
      app->SetStopTime (Seconds (10.0));
      appContainer.Add (app);
      clientApps[i] = app;
    }

  // Traces
  if (enableTracing)
    {
      AsciiTraceHelper ascii;
      for (uint32_t i = 0; i < nClients; ++i)
        {
          std::ostringstream ofname;
          ofname << "star-p2p-" << i << ".tr";
          p2p.EnableAsciiAll (ascii.CreateFileStream (ofname.str ()));
          p2p.EnablePcapAll (std::string ("star-p2p-") + std::to_string (i), true);
        }
    }

  // NetAnim
  AnimationInterface anim ("star-topology.xml");

  anim.SetConstantPosition (serverNode.Get (0), 50.0, 50.0);
  double radius = 30.0;
  for (uint32_t i = 0; i < nClients; ++i)
    {
      double angle = 2 * M_PI * i / nClients;
      double x = 50.0 + radius * std::cos (angle);
      double y = 50.0 + radius * std::sin (angle);
      anim.SetConstantPosition (clientNodes.Get (i), x, y);
    }

  Simulator::Stop (Seconds (12.0));
  Simulator::Run ();

  // Results
  double simTimeSec = 10.0; // Duration of traffic
  double throughputMbps = (g_totalRx * 8.0) / (simTimeSec * 1000000.0);

  double avgLatency = 0.0;
  if (!g_latencies.empty ())
    {
      double sum = 0.0;
      for (double l : g_latencies) sum += l;
      avgLatency = sum / g_latencies.size ();
    }

  std::cout << "========== Simulation Results ==========" << std::endl;
  std::cout << "Total Packets Sent: " << (nClients * numPackets) << std::endl;
  std::cout << "Total Bytes Sent: " << g_totalTx << std::endl;
  std::cout << "Total Bytes Received: " << g_totalRx << std::endl;
  std::cout << "Throughput: " << throughputMbps << " Mbps" << std::endl;
  std::cout << "Average Latency: " << avgLatency << " ms" << std::endl;
  std::cout << "Packets Lost: " << (nClients * numPackets - g_latencies.size ()) << std::endl;
  std::cout << "Total Packets Arrived at Server: " << g_latencies.size () << std::endl;
  std::cout << "========================================" << std::endl;

  Simulator::Destroy ();
  return 0;
}