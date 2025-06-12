#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiDistancePerformance");

static uint32_t g_txPackets = 0;
static uint32_t g_rxPackets = 0;
static uint64_t g_rxBytes = 0;
static double g_delaySum = 0.0;

void
TxCallback (Ptr<const Packet> packet)
{
  ++g_txPackets;
}

void
RxCallback (Ptr<const Packet> packet, const Address &address, const Address &dest)
{
  ++g_rxPackets;
  g_rxBytes += packet->GetSize ();
}

void
RxTraceWithDelay (Ptr<const Packet> packet, const Address &address, const Address &dest, Time rxTime, Time txTime)
{
  ++g_rxPackets;
  g_rxBytes += packet->GetSize ();
  if (rxTime >= txTime)
    {
      g_delaySum += (rxTime - txTime).GetSeconds ();
    }
}

class PacketTimestampHeader : public Header
{
public:
  PacketTimestampHeader() : m_timestamp(Seconds(0)) {}
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("PacketTimestampHeader")
      .SetParent<Header>()
      .AddConstructor<PacketTimestampHeader>();
    return tid;
  }
  virtual TypeId GetInstanceTypeId() const
  {
    return GetTypeId();
  }
  virtual void Serialize(Buffer::Iterator start) const
  {
    int64_t t = m_timestamp.GetNanoSeconds();
    start.WriteHtonU64(t);
  }
  virtual uint32_t GetSerializedSize() const
  {
    return 8;
  }
  virtual uint32_t Deserialize(Buffer::Iterator start)
  {
    int64_t t = start.ReadNtohU64();
    m_timestamp = NanoSeconds(t);
    return 8;
  }
  virtual void Print(std::ostream &os) const
  {
    os << "timestamp=" << m_timestamp.GetSeconds();
  }
  void SetTimestamp(Time t)
  {
    m_timestamp = t;
  }
  Time GetTimestamp() const
  {
    return m_timestamp;
  }
private:
  Time m_timestamp;
};

class TimestampSenderApp : public Application
{
public:
  TimestampSenderApp() : m_socket(0), m_peer(), m_pktSize(0), m_nPkts(0), m_interval(Seconds(1.0)), m_sent(0) {}
  void Setup(Address address, uint32_t pktSize, uint32_t nPkts, Time interval)
  {
    m_peer = address;
    m_pktSize = pktSize;
    m_nPkts = nPkts;
    m_interval = interval;
  }
private:
  virtual void StartApplication(void)
  {
    if (!m_socket)
      {
        m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
        m_socket->Connect(m_peer);
      }
    m_sent = 0;
    SendPacket();
  }
  virtual void StopApplication(void)
  {
    if (m_socket)
      {
        m_socket->Close();
      }
  }
  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_pktSize > 8 ? m_pktSize - 8 : 0);
    PacketTimestampHeader hdr;
    hdr.SetTimestamp(Simulator::Now());
    packet->AddHeader(hdr);
    m_socket->Send(packet);
    TxCallback(packet);
    if (++m_sent < m_nPkts)
      {
        Simulator::Schedule(m_interval, &TimestampSenderApp::SendPacket, this);
      }
  }
  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_pktSize;
  uint32_t m_nPkts;
  Time m_interval;
  uint32_t m_sent;
};

class TimestampReceiverApp : public Application
{
public:
  TimestampReceiverApp() : m_socket(0) {}
  void Setup(Address myAddress)
  {
    m_myAddress = myAddress;
  }
private:
  virtual void StartApplication(void)
  {
    if (!m_socket)
      {
        m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
        InetSocketAddress addr = InetSocketAddress::ConvertFrom(m_myAddress);
        m_socket->Bind(addr);
        m_socket->SetRecvCallback(MakeCallback(&TimestampReceiverApp::HandleRead, this));
      }
  }
  virtual void StopApplication(void)
  {
    if (m_socket)
      {
        m_socket->Close();
      }
  }
  void HandleRead(Ptr<Socket> socket)
  {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    PacketTimestampHeader hdr;
    if (packet->PeekHeader(hdr) == 8)
      {
        packet->RemoveHeader(hdr);
        Time txTimestamp = hdr.GetTimestamp();
        Time rxTimestamp = Simulator::Now();
        RxTraceWithDelay(packet, from, m_myAddress, rxTimestamp, txTimestamp);
      }
    else
      {
        RxCallback(packet, from, m_myAddress);
      }
  }
  Ptr<Socket> m_socket;
  Address m_myAddress;
};

int
main (int argc, char *argv[])
{
  double distance = 50.0;
  uint32_t runId = 1;
  std::string outputFormat = "sqlite";
  uint32_t pktSize = 1024;
  uint32_t pktRate = 100; // pkts/sec
  uint32_t pktCount = 1000;
  double simulationTime = 15.0;

  CommandLine cmd;
  cmd.AddValue ("distance", "Distance between nodes (meters)", distance);
  cmd.AddValue ("format", "Output format: 'sqlite' or 'omnet'", outputFormat);
  cmd.AddValue ("run", "Run identifier", runId);
  cmd.AddValue ("pktSize", "Packet size (bytes)", pktSize);
  cmd.AddValue ("pktRate", "Packet rate (pkts/sec)", pktRate);
  cmd.AddValue ("pktCount", "Packet count", pktCount);
  cmd.AddValue ("simTime", "Simulation time (s)", simulationTime);
  cmd.Parse (argc, argv);

  SeedManager::SetSeed (12345);
  SeedManager::SetRun (runId);

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
  Ptr<MobilityModel> mob0 = nodes.Get(0)->GetObject<MobilityModel>();
  Ptr<MobilityModel> mob1 = nodes.Get(1)->GetObject<MobilityModel>();
  mob0->SetPosition(Vector(0.0, 0.0, 0.0));
  mob1->SetPosition(Vector(distance, 0.0, 0.0));

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces = ipv4.Assign (devices);

  // Timestamp-enabled UDP sender and receiver apps
  uint16_t udpPort = 50000;
  Ptr<TimestampSenderApp> sender = CreateObject<TimestampSenderApp>();
  Address dstAddr (InetSocketAddress (ifaces.GetAddress(1), udpPort));
  sender->Setup (dstAddr, pktSize, pktCount, Seconds (1.0 / pktRate));
  nodes.Get(0)->AddApplication(sender);
  sender->SetStartTime(Seconds(1.0));
  sender->SetStopTime(Seconds(simulationTime));

  Ptr<TimestampReceiverApp> receiver = CreateObject<TimestampReceiverApp>();
  Address myAddr (InetSocketAddress (Ipv4Address::GetAny(), udpPort));
  receiver->Setup (myAddr);
  nodes.Get(1)->AddApplication(receiver);
  receiver->SetStartTime(Seconds(0.0));
  receiver->SetStopTime(Seconds(simulationTime));

  Simulator::Stop(Seconds(simulationTime));

  DataCollector collector;
  collector.SetExperimentId("wifi-distance");
  collector.SetRunId("run-" + std::to_string(runId));
  collector.SetInputFilename("");
  collector.SetOutputFilename((outputFormat == "omnet") ? "wifi-distance.omnet" : "wifi-distance.db");

  // Experiment metadata
  collector.AddMetadata ("Distance", std::to_string(distance));
  collector.AddMetadata ("PacketSize", std::to_string(pktSize));
  collector.AddMetadata ("PacketRate", std::to_string(pktRate));
  collector.AddMetadata ("PacketCount", std::to_string(pktCount));
  collector.AddMetadata ("SimulationTime", std::to_string(simulationTime));
  collector.AddMetadata ("RunId", std::to_string(runId));

  // Schedule stats collection at simulation end
  Simulator::Schedule (Seconds(simulationTime), [&](){
    DataOutputCallback output;
    if (outputFormat == "omnet")
      {
        output = Create<DataOutputOmnet> ();
      }
    else
      {
        output = Create<DataOutputDatabase> ();
      }
    DataSet ds;
    ds.SetTitle ("Wifi distance throughput, delay and delivery ratio");
    ds.AddColumn ("TxPackets", DataSet::UINT32);
    ds.AddColumn ("RxPackets", DataSet::UINT32);
    ds.AddColumn ("RxBytes", DataSet::UINT64);
    ds.AddColumn ("DeliveryRatio", DataSet::DOUBLE);
    ds.AddColumn ("AverageDelaySeconds", DataSet::DOUBLE);
    ds.AddColumn ("ThroughputMbps", DataSet::DOUBLE);

    double deliveryRatio = g_txPackets ? (double)g_rxPackets / g_txPackets : 0.0;
    double averageDelay = g_rxPackets ? g_delaySum / g_rxPackets : 0.0;
    double throughput = (simulationTime > 0) ? (g_rxBytes * 8.0 / 1e6) / simulationTime : 0.0;

    DataValueList values;
    values.Add (g_txPackets);
    values.Add (g_rxPackets);
    values.Add (g_rxBytes);
    values.Add (deliveryRatio);
    values.Add (averageDelay);
    values.Add (throughput);
    ds.AddData (values);
    collector.AddDataSet(ds);

    NS_LOG_UNCOND ("Tx: " << g_txPackets << ", Rx: " << g_rxPackets << ", Bytes: " << g_rxBytes
        << ", Delivery ratio: " << deliveryRatio << ", Average delay: " << averageDelay
        << " s, Throughput: " << throughput << " Mbps");

    output->Output (collector);
  });

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}