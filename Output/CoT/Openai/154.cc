#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <string>

using namespace ns3;

struct PacketRecord
{
  uint32_t srcNode;
  uint32_t dstNode;
  uint32_t packetSize;
  double   txTime;
  double   rxTime;
};

std::vector<PacketRecord> packetRecords;
std::map<uint64_t, std::pair<uint32_t/*src*/, double/*txTime*/> > packetInfo;

void TxTrace(Ptr<const Packet> packet, Mac48Address, Mac48Address, WifiTxVector, MpduInfo, SignalNoiseDbm, uint16_t, Ptr<const WifiPsdu>, uint16_t, Ptr<NetDevice> dev)
{
  uint64_t uid = packet->GetUid();
  uint32_t nodeId = dev->GetNode()->GetId();
  packetInfo[uid] = std::make_pair(nodeId, Simulator::Now().GetSeconds());
}

void RxAppTrace(Ptr<const Packet> packet, const Address &from, uint16_t port)
{
  uint64_t uid = packet->GetUid();
  uint32_t dstNode = 4;
  auto it = packetInfo.find(uid);
  if (it != packetInfo.end())
    {
      PacketRecord rec;
      rec.srcNode = it->second.first;
      rec.dstNode = dstNode;
      rec.packetSize = packet->GetSize();
      rec.txTime = it->second.second;
      rec.rxTime = Simulator::Now().GetSeconds();
      packetRecords.push_back(rec);
      packetInfo.erase(it);
    }
}

int main(int argc, char *argv[])
{
  uint32_t numNodes = 5;
  double simulationTime = 10.0;
  uint32_t packetSize = 512;
  uint32_t packetsPerNode = 100;
  double interval = simulationTime / packetsPerNode;

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(numNodes);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);

  WifiMacHelper mac;
  Ssid ssid = Ssid("wifi-simple");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer devices;
  for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
      devices.Add(wifi.Install(phy, mac, nodes.Get(i)));
    }

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  devices.Add(wifi.Install(phy, mac, nodes.Get(numNodes - 1)));

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
  posAlloc->Add(Vector(0.0, 0.0, 0.0));
  posAlloc->Add(Vector(5.0, 0.0, 0.0));
  posAlloc->Add(Vector(0.0, 5.0, 0.0));
  posAlloc->Add(Vector(5.0, 5.0, 0.0));
  posAlloc->Add(Vector(2.5, 2.5, 0.0));
  mobility.SetPositionAllocator(posAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t serverPort = 9000;
  UdpServerHelper server(serverPort);
  ApplicationContainer serverApps = server.Install(nodes.Get(4));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simulationTime + 1));

  std::vector<ApplicationContainer> clientApps(numNodes - 1);
  for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
      UdpClientHelper client(interfaces.GetAddress(4), serverPort);
      client.SetAttribute("MaxPackets", UintegerValue(packetsPerNode));
      client.SetAttribute("Interval", TimeValue(Seconds(interval)));
      client.SetAttribute("PacketSize", UintegerValue(packetSize));
      clientApps[i] = client.Install(nodes.Get(i));
      clientApps[i].Start(Seconds(0.1));
      clientApps[i].Stop(Seconds(simulationTime));
    }

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/TxMpdu",
                  MakeCallback(&TxTrace));
  Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(serverApps.Get(0));
  udpServer->TraceConnectWithoutContext("RxWithAddresses", MakeCallback(&RxAppTrace));

  Simulator::Stop(Seconds(simulationTime + 2));
  Simulator::Run();

  std::ofstream out("packet_dataset.csv");
  out << "SrcNode,DstNode,PacketSize,TxTime,RxTime\n";
  for (const auto& rec : packetRecords)
    {
      out << rec.srcNode << ',' << rec.dstNode << ',' << rec.packetSize << ','
          << rec.txTime << ',' << rec.rxTime << '\n';
    }
  out.close();

  Simulator::Destroy();
  return 0;
}