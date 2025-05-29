#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <string>

using namespace ns3;

struct PacketRecord
{
  uint32_t sourceNode;
  uint32_t destNode;
  uint32_t packetSize;
  double sentTime;
  double recvTime;
};

class CsvTracer
{
public:
  CsvTracer(const std::string &filename) : m_of(filename, std::ofstream::out)
  {
    m_of << "Source,Destination,PacketSize,TxTime,RxTime\n";
  }

  void AddRecord(const PacketRecord &rec)
  {
    m_of << rec.sourceNode << "," << rec.destNode << "," << rec.packetSize << "," << rec.sentTime << "," << rec.recvTime << "\n";
  }

  ~CsvTracer() { m_of.close(); }

private:
  std::ofstream m_of;
};

class PacketEventTracer
{
public:
  PacketEventTracer(CsvTracer *csvTracer, uint32_t serverNodeId)
      : m_csv(csvTracer), m_serverId(serverNodeId)
  {}

  void PacketTx(Ptr<const Packet> pkt, const Address &srcAddr)
  {
    uint64_t pId = pkt->GetUid();
    uint32_t size = pkt->GetSize();
    m_txTime[pId] = std::make_pair(Simulator::Now().GetSeconds(), size);
    m_txNode[pId] = NodeList::GetNodeIdFromContext(Simulator::GetContext());
  }

  void PacketRx(Ptr<const Packet> pkt, const Address &addr)
  {
    uint64_t pId = pkt->GetUid();
    double rxTime = Simulator::Now().GetSeconds();
    auto it = m_txTime.find(pId);
    if (it != m_txTime.end())
    {
      PacketRecord r;
      r.sourceNode = m_txNode[pId];
      r.destNode = m_serverId;
      r.packetSize = it->second.second;
      r.sentTime = it->second.first;
      r.recvTime = rxTime;
      m_csv->AddRecord(r);
      m_txTime.erase(it);
      m_txNode.erase(pId);
    }
  }

private:
  CsvTracer *m_csv;
  uint32_t m_serverId;
  std::map<uint64_t, std::pair<double, uint32_t>> m_txTime;
  std::map<uint64_t, uint32_t> m_txNode;
};

NS_LOG_COMPONENT_DEFINE("WirelessUdpCsvTracer");

int main(int argc, char *argv[])
{
  uint32_t numNodes = 5;
  double simTime = 10.0;
  std::string csvFile = "packet_dataset.csv";

  CommandLine cmd;
  cmd.AddValue("output", "CSV file output", csvFile);
  cmd.Parse(argc, argv);

  CsvTracer csvTracer(csvFile);

  NodeContainer nodes;
  nodes.Create(numNodes);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(Ssid("ns3-ssid")),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));
  for (uint32_t i = 1; i < numNodes - 1; ++i)
  {
    NetDeviceContainer tmp = wifi.Install(wifiPhy, wifiMac, nodes.Get(i));
    staDevices.Add(tmp.Get(0));
  }

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(Ssid("ns3-ssid")));

  NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, nodes.Get(numNodes - 1));

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(10.0, 0.0, 0.0));
  positionAlloc->Add(Vector(0.0, 10.0, 0.0));
  positionAlloc->Add(Vector(-10.0, 0.0, 0.0));
  positionAlloc->Add(Vector(0.0, 0.0, 10.0)); // central server
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  NetDeviceContainer devices;
  for (uint32_t i = 0; i < staDevices.GetN(); ++i)
    devices.Add(staDevices.Get(i));
  devices.Add(apDevice.Get(0));
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 4000;
  ApplicationContainer serverApps;
  UdpServerHelper server(port);
  serverApps.Add(server.Install(nodes.Get(numNodes - 1)));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simTime + 1));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numNodes - 1; ++i)
  {
    UdpClientHelper client(interfaces.GetAddress(numNodes - 1), port);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(50 + i * 5)));
    client.SetAttribute("PacketSize", UintegerValue(200 + i * 50));
    clientApps.Add(client.Install(nodes.Get(i)));
  }
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(simTime));

  PacketEventTracer pktTracer(&csvTracer, nodes.Get(numNodes-1)->GetId());

  for (uint32_t i = 0; i < numNodes - 1; ++i)
  {
    Ptr<NetDevice> dev = nodes.Get(i)->GetDevice(0);
    dev->TraceConnectWithoutContext("PhyTxEnd",
      MakeCallback([&, i](Ptr<const Packet> pkt)
    {
      pktTracer.PacketTx(pkt, Address());
    }));
  }

  Ptr<Node> serverNode = nodes.Get(numNodes - 1);
  Ptr<NetDevice> apDev = serverNode->GetDevice(0);
  apDev->TraceConnectWithoutContext("PhyRxEnd",
    MakeCallback([&](Ptr<const Packet> pkt)
  {
    pktTracer.PacketRx(pkt, Address());
  }));

  Simulator::Stop(Seconds(simTime + 1));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}