#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VANETWifi80211pExample");

uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;
double totalDelay = 0.0;

void RxPacket (Ptr<const Packet> packet, const Address &address, const Ptr<Socket> &socket)
{
  packetsReceived++;
  SocketIpHeader ipHeader;
  packet->PeekHeader (ipHeader);
  Time now = Simulator::Now ();
  if (packet->PeekPacketTag (ipHeader))
    {
      totalDelay += (now.GetSeconds () - ipHeader.GetTtl ());
    }
}

void TxPacket (Ptr<const Packet> packet)
{
  packetsSent++;
}

void PacketSentCallback (Ptr<const Packet> packet)
{
  packetsSent++;
}

void PacketReceivedCallback (Ptr<const Packet> packet, const Address &address)
{
  packetsReceived++;
}

void ApplicationRxCallback (Ptr<const Packet> packet, const Address &address)
{
  packetsReceived++;
}

class UdpClientAppWithTimestamp : public UdpClient
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("UdpClientAppWithTimestamp")
      .SetParent<UdpClient> ()
      .SetGroupName("Applications")
      ;
    return tid;
  }

  virtual void Send (void) override
  {
    Ptr<Packet> p = Create<Packet> (m_size);
    // Add time as a custom PacketTag
    TimeTag timeTag;
    timeTag.SetTimestamp (Simulator::Now ());
    p->AddPacketTag (timeTag);

    if (m_socket)
      {
        m_socket->SendTo (p, 0, m_peerAddress);
      }

    ++m_sent;
    if (m_sent < m_count)
      {
        ScheduleTransmit (m_interval);
      }
  }
};

class UdpServerAppWithTimestamp : public UdpServer
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("UdpServerAppWithTimestamp")
      .SetParent<UdpServer> ()
      .SetGroupName("Applications")
      ;
    return tid;
  }

private:
  virtual void HandleRead (Ptr<Socket> socket) override
  {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom (from);
    TimeTag timeTag;
    if (packet->PeekPacketTag (timeTag))
      {
        Time sent = timeTag.GetTimestamp ();
        double delay = Simulator::Now ().GetSeconds () - sent.GetSeconds ();
        totalDelay += delay;
      }
    packetsReceived++;

    while (socket->GetRxAvailable () > 0)
      {
        packet = socket->RecvFrom (from);
        if (packet->PeekPacketTag (timeTag))
          {
            Time sent = timeTag.GetTimestamp ();
            double delay = Simulator::Now ().GetSeconds () - sent.GetSeconds ();
            totalDelay += delay;
          }
        packetsReceived++;
      }
  }
};

int main (int argc, char *argv[])
{
  uint32_t numVehicles = 5;
  double simulationTime = 20.0; // seconds
  double txInterval = 1.0; // seconds

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer vehicles;
  vehicles.Create (numVehicles);

  // Install mobility: straight line, same direction, different speeds
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  double initialX = 0.0;
  double roadY = 100.0;
  double spacing = 30.0; // meters between vehicles
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      positionAlloc->Add (Vector (initialX + i * spacing, roadY, 0));
    }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (vehicles);

  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      mob->SetVelocity (Vector (20.0 + i*5.0, 0.0, 0.0)); // Different speeds: 20,25,30,35,40 m/s
    }

  // Configure 802.11p PHY and MAC
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
  wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode",StringValue ("OfdmRate6MbpsBW10MHz"),
                                      "ControlMode",StringValue ("OfdmRate6MbpsBW10MHz"));

  NetDeviceContainer devices = wifi80211p.Install (wifiPhy, wifi80211pMac, vehicles);

  // Install internet stack
  InternetStackHelper stack;
  stack.Install (vehicles);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // UDP server on Node 0
  uint16_t serverPort = 4000;
  Ptr<UdpServerAppWithTimestamp> serverApp = CreateObject<UdpServerAppWithTimestamp> ();
  serverApp->SetAttribute ("Port", UintegerValue (serverPort));
  vehicles.Get (0)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (1.0));
  serverApp->SetStopTime (Seconds (simulationTime));

  // UDP clients on Nodes 1-4
  for (uint32_t i = 1; i < numVehicles; ++i)
    {
      Ptr<UdpClientAppWithTimestamp> client = CreateObject<UdpClientAppWithTimestamp> ();
      client->SetAttribute ("RemoteAddress", AddressValue (interfaces.GetAddress (0)));
      client->SetAttribute ("RemotePort", UintegerValue (serverPort));
      client->SetAttribute ("MaxPackets", UintegerValue (1000));
      client->SetAttribute ("Interval", TimeValue (Seconds (txInterval)));
      client->SetAttribute ("PacketSize", UintegerValue (512));
      vehicles.Get (i)->AddApplication (client);
      client->SetStartTime (Seconds (2.0 + i * 0.1)); // Staggered start
      client->SetStopTime (Seconds (simulationTime));
    }

  // NetAnim setup
  AnimationInterface anim ("vanet-80211p.xml");
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      std::ostringstream oss;
      oss << "Vehicle-" << i;
      anim.UpdateNodeDescription(vehicles.Get(i), oss.str());
      anim.UpdateNodeColor(vehicles.Get(i), 0, ((i+1)*50)%255, 255 - ((i+1)*50)%255);
    }
  anim.EnablePacketMetadata (true);

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Metrics calculation
  uint32_t totalSent = 0;
  for (uint32_t i = 1; i < numVehicles; ++i)
    {
      Ptr<UdpClientAppWithTimestamp> client = DynamicCast<UdpClientAppWithTimestamp> (vehicles.Get(i)->GetApplication(0));
      totalSent += client->GetTotalTx ();
    }

  uint32_t totalReceived = serverApp->GetReceived ();
  double pdr = (totalSent > 0) ? ((double)totalReceived / totalSent) * 100.0 : 0.0;
  double averageDelay = (totalReceived > 0) ? (totalDelay / totalReceived) : 0.0;

  std::cout << "Total Packets Sent: " << totalSent << std::endl;
  std::cout << "Total Packets Received: " << totalReceived << std::endl;
  std::cout << "Packet Delivery Ratio: " << pdr << " %" << std::endl;
  std::cout << "Average End-to-End Delay: " << averageDelay << " s" << std::endl;

  Simulator::Destroy ();
  return 0;
}

// PacketTag for timestamping
class TimeTag : public Tag
{
public:
  TimeTag () : m_timestamp (Seconds (0)) {}
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
    int64_t t = m_timestamp.GetNanoSeconds ();
    i.Write ((const uint8_t *)&t, sizeof (t));
  }
  virtual void Deserialize (TagBuffer i)
  {
    int64_t t;
    i.Read ((uint8_t *)&t, sizeof (t));
    m_timestamp = NanoSeconds (t);
  }
  virtual uint32_t GetSerializedSize () const
  {
    return sizeof (int64_t);
  }
  virtual void Print (std::ostream &os) const
  {
    os << "Timestamp=" << m_timestamp.GetSeconds ();
  }
  void SetTimestamp (Time time) { m_timestamp = time; }
  Time GetTimestamp () const { return m_timestamp; }
private:
  Time m_timestamp;
};