/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WSN_Csma_Example");

// Metrics collectors
uint32_t g_packetsSent = 0;
uint32_t g_packetsReceived = 0;
double   g_totalDelay = 0.0;

std::map<uint32_t, Time> g_sentTimes;

// Application to periodically send UDP packets to a base station
class PeriodicSender : public Application
{
public:
  PeriodicSender () {}
  virtual ~PeriodicSender () {}

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, Time pktInterval, uint32_t nPackets)
  {
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_pktInterval = pktInterval;
    m_nPackets = nPackets;
    m_packetsSent = 0;
  }

private:
  virtual void StartApplication () override
  {
    m_running = true;
    m_socket->Bind ();
    m_socket->Connect (m_peer);
    SendPacket ();
  }

  virtual void StopApplication () override
  {
    m_running = false;
    if (m_sendEvent.IsRunning ())
      {
        Simulator::Cancel (m_sendEvent);
      }
    if (m_socket)
      {
        m_socket->Close ();
      }
  }

  void SendPacket ()
  {
    if (!m_running || m_packetsSent >= m_nPackets)
      return;

    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    m_socket->Send (packet);

    // For tracking: store send time by Sequence Number for delay computation
    SequenceNumber32 seq (m_packetsSent);
    g_sentTimes[seq.GetValue ()] = Simulator::Now ();

    ++g_packetsSent;
    ++m_packetsSent;

    m_sendEvent = Simulator::Schedule (m_pktInterval, &PeriodicSender::SendPacket, this);
  }

  Ptr<Socket> m_socket;
  Address     m_peer;
  uint32_t    m_packetSize;
  Time        m_pktInterval;
  uint32_t    m_nPackets;
  uint32_t    m_packetsSent;
  EventId     m_sendEvent;
  bool        m_running;
};

// Callback for receiving UDP packets at base station
void
ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      // Sequence number is embedded in the sending order, can use g_packetsReceived as seq num
      uint32_t seq = g_packetsReceived;
      ++g_packetsReceived;
      Time tx = g_sentTimes.count (seq) ? g_sentTimes[seq] : Seconds (0.0);
      double delay = (Simulator::Now () - tx).GetSeconds ();
      g_totalDelay += delay;
    }
}

int
main (int argc, char *argv[])
{
  uint32_t nNodes = 6;
  uint32_t packetSize = 50;
  Time pktInterval = Seconds (2.0);
  uint32_t nPackets = 10;
  NS_ASSERT_MSG (nNodes >= 2, "At least two nodes required.");

  CommandLine cmd;
  cmd.AddValue ("nPackets", "Number of packets per sender", nPackets);
  cmd.AddValue ("pktInterval", "Interval between packets", pktInterval);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (nNodes);

  // The last node is the base station
  uint32_t baseStationIdx = nNodes - 1;

  // Set up CSMA
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer devices = csma.Install (nodes);

  // Mobility: place nodes in a grid
  MobilityHelper mobility;
  mobility.SetPositionAllocator (
    "ns3::GridPositionAllocator",
    "MinX", DoubleValue (0.0),
    "MinY", DoubleValue (0.0),
    "DeltaX", DoubleValue (20.0),
    "DeltaY", DoubleValue (20.0),
    "GridWidth", UintegerValue (3),
    "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Install UDP receiver on base station
  uint16_t port = 4000;
  Ptr<Socket> recvSocket = Socket::CreateSocket (nodes.Get (baseStationIdx), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  recvSocket->Bind (local);
  recvSocket->SetRecvCallback (MakeCallback (&ReceivePacket));

  // Install sender applications on sensor nodes (not base station)
  for (uint32_t i = 0; i < nNodes - 1; ++i)
    {
      Ptr<Socket> sendSocket = Socket::CreateSocket (nodes.Get (i), UdpSocketFactory::GetTypeId ());
      Address destAddr (InetSocketAddress (interfaces.GetAddress (baseStationIdx), port));

      Ptr<PeriodicSender> app = CreateObject<PeriodicSender> ();
      app->Setup (sendSocket, destAddr, packetSize, pktInterval, nPackets);
      nodes.Get (i)->AddApplication (app);
      app->SetStartTime (Seconds (1.0 + i * 0.1)); // stagger sending
      app->SetStopTime (Seconds (1.0 + nPackets * pktInterval.GetSeconds () + 10));
    }

  // NetAnim configuration
  AnimationInterface anim ("wsn-csma-netanim.xml");
  anim.SetBackgroundImage ("", 0, 0, 0, 0, 1.0); // no background image

  // Label base station
  anim.UpdateNodeDescription (baseStationIdx, "Base Station");
  anim.UpdateNodeColor (baseStationIdx, 255, 0, 0); // red for base
  for (uint32_t i = 0; i < nNodes - 1; ++i)
    {
      anim.UpdateNodeDescription (i, "Sensor");
      anim.UpdateNodeColor (i, 0, 255, 0); // green for sensors
    }

  Simulator::Stop (Seconds (1.5 + nPackets * pktInterval.GetSeconds () + 10));

  Simulator::Run ();
  Simulator::Destroy ();

  std::cout << "\n-------- Simulation Results --------\n";
  std::cout << "Packets sent     : " << g_packetsSent << std::endl;
  std::cout << "Packets received : " << g_packetsReceived << std::endl;
  std::cout << "Packet Delivery Ratio: " 
            << (g_packetsSent ? (100.0 * g_packetsReceived / g_packetsSent) : 0.0) << "%\n";
  std::cout << "Average End-to-End Delay: "
            << (g_packetsReceived ? (g_totalDelay / g_packetsReceived) : 0.0) << " s\n";
  std::cout << "------------------------------------\n";

  return 0;
}