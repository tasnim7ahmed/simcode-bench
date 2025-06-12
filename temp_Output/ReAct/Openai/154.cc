#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <string>
#include <unordered_map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiPacketDatasetSimulation");

struct PacketRecord
{
  uint32_t src;
  uint32_t dst;
  uint32_t size;
  double sendTime;
  double recvTime;
};

class PacketLogger
{
public:
  PacketLogger(std::string filename)
  {
    m_file.open(filename, std::ios::out);
    m_file << "src,dst,size,sendTime,recvTime\n";
  }

  ~PacketLogger()
  {
    m_file.close();
  }

  void LogSend(Ptr<const Packet> pkt, uint32_t src, uint32_t dst)
  {
    uint64_t uid = pkt->GetUid();
    double time  = Simulator::Now().GetSeconds();
    m_pending[uid] = {src, dst, pkt->GetSize(), time, 0.0};
  }

  void LogRecv(Ptr<const Packet> pkt)
  {
    uint64_t uid = pkt->GetUid();
    double time  = Simulator::Now().GetSeconds();
    auto it = m_pending.find(uid);
    if (it != m_pending.end())
    {
      it->second.recvTime = time;
      // Write to file
      m_file << it->second.src << ","
             << it->second.dst << ","
             << it->second.size << ","
             << it->second.sendTime << ","
             << it->second.recvTime << "\n";
      m_pending.erase(it);
    }
  }

private:
  std::ofstream m_file;
  std::unordered_map<uint64_t, PacketRecord> m_pending;
};

Ptr<PacketLogger> packetLogger;

void TxCallback(Ptr<const Packet> pkt, const Address &address, uint32_t nodeId, uint32_t dstId)
{
  packetLogger->LogSend(pkt, nodeId, dstId);
}

void RxCallback(Ptr<const Packet> pkt, const Address &address)
{
  packetLogger->LogRecv(pkt);
}

int main(int argc, char *argv[])
{
  CommandLine cmd;
  std::string csvFile = "packet_dataset.csv";
  cmd.AddValue("csvFile", "CSV output file", csvFile);
  cmd.Parse(argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(5);

  // Install Wifi devices
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
    "DataMode", StringValue("DsssRate11Mbps"),
    "ControlMode", StringValue("DsssRate11Mbps"));

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices;
  for(uint32_t i=0; i<4; ++i)
  {
    staDevices.Add(wifi.Install(phy, mac, nodes.Get(i)));
  }

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice;
  apDevice.Add(wifi.Install(phy, mac, nodes.Get(4)));

  NetDeviceContainer devices;
  devices.Add(staDevices);
  devices.Add(apDevice);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
  posAlloc->Add(Vector(0.0, 3.0, 0.0));
  posAlloc->Add(Vector(3.0, 0.0, 0.0));
  posAlloc->Add(Vector(0.0, -3.0, 0.0));
  posAlloc->Add(Vector(-3.0, 0.0, 0.0));
  posAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP/server at center
  mobility.SetPositionAllocator(posAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Server (node 4)
  uint16_t srvPort = 9000;
  UdpServerHelper server(srvPort);
  ApplicationContainer srvApp = server.Install(nodes.Get(4));
  srvApp.Start(Seconds(0.0));
  srvApp.Stop(Seconds(15.0));

  // Install UDP Client on each other node
  uint32_t pktSize = 512;
  double interPkt = 1.0;
  for (uint32_t i=0; i<4; ++i)
  {
    UdpClientHelper client(interfaces.GetAddress(4), srvPort);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(interPkt)));
    client.SetAttribute("PacketSize", UintegerValue(pktSize));
    ApplicationContainer clientApp = client.Install(nodes.Get(i));
    clientApp.Start(Seconds(1.0 + 0.1*i));
    clientApp.Stop(Seconds(14.0));
  }

  // Packet Logger
  packetLogger = CreateObject<PacketLogger>(csvFile);

  // Connect trace sources for packet send (down to device) and reception (up from device)
  for(uint32_t i=0; i<4; ++i)
  {
    Ptr<NetDevice> dev = staDevices.Get(i);
    dev->TraceConnect("PhyTxEnd", 
      MakeBoundCallback(&TxCallback, i, 4));
  }
  apDevice.Get(0)->TraceConnectWithoutContext("PhyRxEnd", 
    MakeCallback(&RxCallback));

  Simulator::Stop(Seconds(15.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}