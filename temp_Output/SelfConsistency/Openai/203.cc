/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * VANET simulation with 5 vehicles using 802.11p and UDP client-server communication.
 * Gathers PDR and end-to-end delay metrics, and enables NetAnim visualization.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Vanet80211pUdpExample");

// Custom application metrics collector
class MetricsCollector : public Object
{
public:
  MetricsCollector ()
    : m_rxCount (0),
      m_txCount (0),
      m_totalDelay (Seconds (0))
  {}

  void TxPacket ()
  {
    ++m_txCount;
  }

  void RxPacket (Ptr<const Packet> packet, const Address &from)
  {
    ++m_rxCount;
    // Extract and accumulate delay
    uint64_t tSent;
    if (packet->PeekPacketTag (tSent))
      {
        Time sent = TimeStep (tSent);
        Time delay = Simulator::Now () - sent;
        m_totalDelay += delay;
      }
    else if (packet->PeekPacketTag<TimeTag> ())
      {
        TimeTag tag;
        packet->PeekPacketTag (tag);
        Time sent = tag.GetTime ();
        Time delay = Simulator::Now () - sent;
        m_totalDelay += delay;
      }
  }

  void RxPacketDelayTagged (Ptr<const Packet> packet, const Address &from)
  {
    // Tag stores the time packet was sent.
    TimeTag tag;
    if (packet->PeekPacketTag (tag))
      {
        ++m_rxCount;
        m_totalDelay += Simulator::Now () - tag.GetTime ();
      }
  }

  void SetNumClients (uint32_t n)
  {
    m_numClients = n;
  }

  void PrintResults ()
  {
    double pdr = (m_txCount > 0) ? 100.0 * m_rxCount / m_txCount : 0.0;
    double avgDelayMs = (m_rxCount > 0) ? m_totalDelay.GetMilliSeconds () / m_rxCount : 0.0;

    std::cout << "==== Simulation Results ====" << std::endl;
    std::cout << "Packets sent:    " << m_txCount << std::endl;
    std::cout << "Packets received: " << m_rxCount << std::endl;
    std::cout << "Packet Delivery Ratio (PDR): " << pdr << " %" << std::endl;
    std::cout << "Average end-to-end delay:    " << avgDelayMs << " ms" << std::endl;
    std::cout << "=============================" << std::endl;
  }

  void AddDelay (Time d)
  {
    m_totalDelay += d;
  }

  void AddRx ()
  {
    ++m_rxCount;
  }

  void AddTx ()
  {
    ++m_txCount;
  }

  uint32_t m_rxCount;
  uint32_t m_txCount;
  Time m_totalDelay;
  uint32_t m_numClients;
};

// Tag packets with time sent
class TimeTag : public Tag
{
public:
  TimeTag () : m_time (Seconds (0)) {}

  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("TimeTag")
      .SetParent<Tag> ()
      .AddConstructor<TimeTag> ();
    return tid;
  }
  virtual TypeId GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }

  virtual void Serialize (TagBuffer i) const
  {
    int64_t t = int64_t (m_time.GetNanoSeconds ());
    i.Write ((const uint8_t*) &t, sizeof(t));
  }
  virtual void Deserialize (TagBuffer i)
  {
    int64_t t;
    i.Read ((uint8_t*) &t, sizeof(t));
    m_time = NanoSeconds (t);
  }
  virtual uint32_t GetSerializedSize () const
  {
    return sizeof (int64_t);
  }
  virtual void Print (std::ostream &os) const
  {
    os << "t=" << m_time.GetSeconds () << "s";
  }

  void SetTime (Time t)
  {
    m_time = t;
  }
  Time GetTime () const
  {
    return m_time;
  }

private:
  Time m_time;
};

// Tag the outgoing packets with current time
void
TxPacketTrace (Ptr<MetricsCollector> metrics, Ptr<const Packet> packet)
{
  metrics->AddTx ();
}

// Tag the outgoing packets with current time
void
UdpClientTxTrace (Ptr<MetricsCollector> metrics, Ptr<Packet> packet)
{
  TimeTag tag;
  tag.SetTime (Simulator::Now ());
  packet->AddPacketTag (tag);
  metrics->AddTx ();
}

// Register for Rx packets at the server, collect time and compute delay
void
UdpServerRxTrace (Ptr<MetricsCollector> metrics,
                  Ptr<const Packet> packet, const Address &from)
{
  TimeTag tag;
  if (packet->PeekPacketTag (tag))
    {
      Time sent = tag.GetTime ();
      Time delay = Simulator::Now () - sent;
      metrics->AddDelay (delay);
      metrics->AddRx ();
    }
}

int
main (int argc, char *argv[])
{
  uint32_t numVehicles = 5;
  double simulationTime = 15.0; // seconds
  uint32_t packetSize = 512; // bytes
  uint32_t clientSendIntervalMs = 500; // ms
  uint16_t udpPort = 4000;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("numVehicles", "Number of vehicles", numVehicles);
  cmd.Parse (argc, argv);

  NodeContainer vehicles;
  vehicles.Create (numVehicles);

  // Install Wi-Fi 802.11p/WAVE
  YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default ();
  wavePhy.SetChannel (waveChannel.Create ());
  wavePhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11);

  NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper waveHelper = Wifi80211pHelper::Default ();
  waveHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue ("OfdmRate6MbpsBW10MHz"),
                                      "ControlMode", StringValue ("OfdmRate6MbpsBW10MHz"));

  NetDeviceContainer deviceContainer = waveHelper.Install (wavePhy, waveMac, vehicles);

  // Mobility: position vehicles on X-axis with 40m gap, assign initial speeds 15-22 m/s
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      positionAlloc->Add (Vector (i * 40.0, 0.0, 0.0));
    }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");

  mobility.Install (vehicles);
  // Set different speed for each vehicle (all in +X direction)
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      Ptr<ConstantVelocityMobilityModel> mob =
        vehicles.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      double speed = 15.0 + (i * 1.8); // 15, 16.8, 18.6, 20.4, 22.2 m/s
      mob->SetVelocity (Vector (speed, 0.0, 0.0));
    }

  // Install stack
  InternetStackHelper internet;
  internet.Install (vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaceContainer = ipv4.Assign (deviceContainer);

  // Set up UDP server on vehicle 0 (the first vehicle)
  UdpServerHelper udpServer (udpPort);
  ApplicationContainer serverApps = udpServer.Install (vehicles.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  // Setup UDP clients on other vehicles
  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < numVehicles; ++i)
    {
      UdpClientHelper udpClient (ifaceContainer.GetAddress (0), udpPort);
      udpClient.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
      udpClient.SetAttribute ("Interval", TimeValue (MilliSeconds (clientSendIntervalMs)));
      udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
      clientApps.Add (udpClient.Install (vehicles.Get (i)));
    }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  // Set up collector for metrics, trace hooks
  Ptr<MetricsCollector> metrics = CreateObject<MetricsCollector> ();
  metrics->SetNumClients (numVehicles - 1);

  // Trace client transmissions (tag packets)
  for (uint32_t i = 1; i < numVehicles; ++i)
    {
      Ptr<UdpClient> client = DynamicCast<UdpClient> (clientApps.Get (i - 1));
      client->TraceConnectWithoutContext ("Tx",
        MakeBoundCallback (&UdpClientTxTrace, metrics));
    }

  // Trace received packets at the server
  Ptr<UdpServer> server = DynamicCast<UdpServer> (serverApps.Get (0));
  server->TraceConnectWithoutContext ("Rx",
    MakeBoundCallback (&UdpServerRxTrace, metrics));

  // Enable pcap and NetAnim tracing
  wavePhy.EnablePcap ("vanet-80211p", deviceContainer, true);

  AnimationInterface anim ("vanet-80211p.xml");
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      anim.UpdateNodeDescription (vehicles.Get (i), "Vehicle_" + std::to_string (i));
      anim.UpdateNodeColor (vehicles.Get (i), 0, 255, 0); // Green
    }
  anim.EnableWireless ();

  Simulator::Stop (Seconds (simulationTime));

  Simulator::Run ();

  metrics->PrintResults ();

  Simulator::Destroy ();

  return 0;
}