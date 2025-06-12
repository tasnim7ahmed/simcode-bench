#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/olsr-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiGridAdhocSimulation");

class AdhocPacketSink : public Application
{
public:
  static TypeId GetTypeId();
  AdhocPacketSink();
  virtual ~AdhocPacketSink();

protected:
  void StartApplication() override;
  void StopApplication() override;

private:
  void ReceivePacket(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  uint64_t m_totalRx;
};

TypeId
AdhocPacketSink::GetTypeId()
{
  static TypeId tid = TypeId("AdhocPacketSink")
                          .SetParent<Application>()
                          .AddConstructor<AdhocPacketSink>();
  return tid;
}

AdhocPacketSink::AdhocPacketSink()
    : m_socket(nullptr), m_totalRx(0)
{
}

AdhocPacketSink::~AdhocPacketSink()
{
}

void AdhocPacketSink::StartApplication()
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&AdhocPacketSink::ReceivePacket, this));
  }
}

void AdhocPacketSink::StopApplication()
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void AdhocPacketSink::ReceivePacket(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    m_totalRx += packet->GetSize();
    NS_LOG_UNCOND("Received packet of size " << packet->GetSize() << " at node " << GetNode()->GetId());
  }
}

int main(int argc, char *argv[])
{
  uint32_t gridRows = 5;
  uint32_t gridCols = 5;
  double distance = 50.0;
  std::string phyMode("DsssRate1Mbps");
  uint32_t packetSize = 1000;
  uint32_t numPackets = 1;
  uint32_t sourceNode = 24;
  uint32_t sinkNode = 0;
  bool verbose = false;
  bool pcapTracing = false;
  bool asciiTracing = false;
  double rssiCutoff = -80;

  CommandLine cmd;
  cmd.AddValue("gridRows", "Number of rows in the grid", gridRows);
  cmd.AddValue("gridCols", "Number of columns in the grid", gridCols);
  cmd.AddValue("distance", "Distance between nodes in meters", distance);
  cmd.AddValue("phyMode", "Wi-Fi Physical layer mode", phyMode);
  cmd.AddValue("packetSize", "Size of application packet sent", packetSize);
  cmd.AddValue("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue("sourceNode", "Node index to use as source", sourceNode);
  cmd.AddValue("sinkNode", "Node index to use as sink", sinkNode);
  cmd.AddValue("verbose", "Turn on all logging", verbose);
  cmd.AddValue("pcap", "Enable pcap tracing", pcapTracing);
  cmd.AddValue("ascii", "Enable ASCII tracing", asciiTracing);
  cmd.AddValue("rssiCutoff", "RSSI cutoff for transmission", rssiCutoff);
  cmd.Parse(argc, argv);

  if (verbose)
  {
    LogComponentEnable("WifiGridAdhocSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));
  Config::SetDefault("ns3::WifiPhy::TxPowerEnd", DoubleValue(16));
  Config::SetDefault("ns3::WifiPhy::TxPowerStart", DoubleValue(16));
  Config::SetDefault("ns3::WifiPhy::EnergyDetectionThreshold", DoubleValue(rssiCutoff + 3));
  Config::SetDefault("ns3::WifiPhy::CcaMode1Threshold", DoubleValue(rssiCutoff));

  NodeContainer nodes;
  nodes.Create(gridRows * gridCols);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue(3.0));

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid("adhoc-network");
  mac.SetType("ns3::AdhocWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer devices = wifi.Install(channel.Create(), mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(distance),
                                "GridWidth", UintegerValue(gridCols),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  OlsrHelper olsr;
  Ipv4ListRoutingHelper list;
  list.Add(olsr, 10);
  stack.SetRoutingHelper(list);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;

  if (sinkNode < nodes.GetN())
  {
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(sinkNode));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(100.0));
  }

  if (sourceNode < nodes.GetN())
  {
    InetSocketAddress destAddr(interfaces.GetAddress(sinkNode), port);
    OnOffHelper onoff("ns3::UdpSocketFactory", destAddr);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("NumberOfPackets", UintegerValue(numPackets));

    ApplicationContainer apps = onoff.Install(nodes.Get(sourceNode));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(100.0));
  }

  if (pcapTracing)
  {
    AsciiTraceHelper ascii;
    wifi.EnableAsciiAll(ascii.CreateFileStream("wifi-grid-adhoc.tr"));
    wifi.EnablePcapAll("wifi-grid-adhoc");
  }

  Simulator::Stop(Seconds(100.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}