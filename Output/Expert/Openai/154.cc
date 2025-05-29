#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;

struct PacketRecord
{
  uint32_t source;
  uint32_t destination;
  uint32_t size;
  double txTime;
  double rxTime;
};

std::vector<PacketRecord> g_packetRecords;
std::map<uint64_t, std::pair<uint32_t, double>> g_packetTxInfo;
std::ofstream g_output;

void TraceTx (Ptr<const Packet> packet, Mac48Address src, Mac48Address dst)
{
  if (packet->GetSize() == 0) return;
  uint64_t uid = packet->GetUid();
  g_packetTxInfo[uid] = std::make_pair (src.GetAddress()[5], Simulator::Now().GetSeconds());
}

void TraceRx (std::string context, Ptr<const Packet> packet, const Address &address)
{
  uint64_t uid = packet->GetUid();
  if (g_packetTxInfo.find(uid) != g_packetTxInfo.end())
    {
      PacketRecord record;
      record.source = g_packetTxInfo[uid].first;
      record.destination = 4;
      record.size = packet->GetSize();
      record.txTime = g_packetTxInfo[uid].second;
      record.rxTime = Simulator::Now().GetSeconds();
      g_packetRecords.push_back(record);
      g_packetTxInfo.erase(uid);
    }
}

int main(int argc, char *argv[])
{
  uint32_t numNodes = 5;
  uint32_t packetSize = 512;
  double simTime = 10.0;
  double dataRateMbps = 2.0;
  double startTime = 1.0;

  NodeContainer nodes;
  nodes.Create(numNodes);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue(ssid),
               "ActiveProbing", BooleanValue(false));
  NetDeviceContainer devices = wifi.Install(phy, mac, nodes.GetN(0), nodes.GetN(1), nodes.GetN(2), nodes.GetN(3));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue(ssid));
  devices.Add(wifi.Install(phy, mac, nodes.Get(4)));

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(5.0, 0.0, 0.0));
  positionAlloc->Add(Vector(10.0, 0.0, 0.0));
  positionAlloc->Add(Vector(15.0, 0.0, 0.0));
  positionAlloc->Add(Vector(7.5, 10.0, 0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 4000;
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(nodes.Get(4));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(simTime + 1.0));

  for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
      UdpClientHelper client(interfaces.GetAddress(4), port);
      client.SetAttribute("MaxPackets", UintegerValue(0));
      client.SetAttribute("Interval", TimeValue(Seconds((packetSize * 8) / (dataRateMbps * 1e6))));
      client.SetAttribute("PacketSize", UintegerValue(packetSize));
      ApplicationContainer clientApp = client.Install(nodes.Get(i));
      clientApp.Start(Seconds(startTime));
      clientApp.Stop(Seconds(simTime));
    }

  // Trace packet transmissions at MAC layer for each station
  for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
      Ptr<NetDevice> dev = devices.Get(i);
      Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
      if (wifiDev)
        {
          wifiDev->GetMac()->TraceConnectWithoutContext(
            "Tx", MakeCallback(&TraceTx));
        }
    }

  // Trace receptions at server's WiFiNetDevice
  Ptr<NetDevice> serverDev = devices.Get(4);
  Ptr<WifiNetDevice> wifiServerDev = DynamicCast<WifiNetDevice>(serverDev);
  if (wifiServerDev)
    {
      wifiServerDev->TraceConnect("MacRx", "", MakeCallback(&TraceRx));
    }

  Simulator::Stop(Seconds(simTime + 1.0));
  Simulator::Run();

  g_output.open("packet_dataset.csv", std::ios::out);
  g_output << "Source,Destination,PacketSize,TxTime,RxTime\n";
  for (const auto &rec : g_packetRecords)
    {
      g_output << rec.source << ','
               << rec.destination << ','
               << rec.size << ','
               << rec.txTime << ','
               << rec.rxTime << '\n';
    }
  g_output.close();

  Simulator::Destroy();
  return 0;
}