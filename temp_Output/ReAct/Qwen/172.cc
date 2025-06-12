#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink-helper.h"
#include <vector>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvAdHocSimulation");

class UdpTrafficHelper {
public:
  void ScheduleUdpTransfer(NodeContainer nodes, uint16_t port);
  void ReceivePacket(Ptr<Socket> socket);
  void RecordTransmission(uint32_t txNodeId, uint32_t seqNum, Time txTime);
  void RecordReception(uint32_t rxNodeId, uint32_t seqNum, Time rxTime);
  void ReportMetrics(void);

private:
  std::map<uint32_t, std::map<uint32_t, Time>> m_txMap;
  std::map<uint32_t, std::map<uint32_t, Time>> m_rxMap;
};

void UdpTrafficHelper::RecordTransmission(uint32_t txNodeId, uint32_t seqNum, Time txTime)
{
  m_txMap[txNodeId][seqNum] = txTime;
}

void UdpTrafficHelper::RecordReception(uint32_t rxNodeId, uint32_t seqNum, Time rxTime)
{
  m_rxMap[rxNodeId][seqNum] = rxTime;
}

void UdpTrafficHelper::ReportMetrics()
{
  uint32_t totalSent = 0;
  uint32_t totalReceived = 0;
  Time totalDelay = Seconds(0);

  for (auto& txNode : m_txMap)
    {
      for (auto& pkt : txNode.second)
        {
          totalSent++;
          auto rxIter = m_rxMap.find(pkt.first);
          if (rxIter != m_rxMap.end())
            {
              auto rxPkt = rxIter->second.find(pkt.first);
              if (rxPkt != rxIter->second.end())
                {
                  totalReceived++;
                  totalDelay += (rxPkt->second - pkt.second);
                }
            }
        }
    }

  double pdr = (totalSent > 0) ? static_cast<double>(totalReceived) / totalSent : 0.0;
  double avgDelay = (totalReceived > 0) ? totalDelay.GetSeconds() / totalReceived : 0.0;

  NS_LOG_UNCOND("Packet Delivery Ratio: " << pdr);
  NS_LOG_UNCOND("Average End-to-End Delay: " << avgDelay << " seconds");
}

void UdpTrafficHelper::ReceivePacket(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
    {
      uint32_t seqNum = 0;
      packet->CopyData(&seqNum, sizeof(seqNum));
      Ipv4Address srcAddr = InetSocketAddress::ConvertFrom(from).GetIpv4();
      uint32_t nodeId = socket->GetNode()->GetId();

      auto ipIter = m_rxMap.find(nodeId);
      if (ipIter == m_rxMap.end())
        {
          std::map<uint32_t, Time> newEntry;
          m_rxMap.insert(std::make_pair(nodeId, newEntry));
        }

      m_rxMap[nodeId][seqNum] = Simulator::Now();

      NS_LOG_DEBUG("Node " << nodeId << " received packet with seq=" << seqNum << " at " << Simulator::Now().GetSeconds());
    }
}

void UdpTrafficHelper::ScheduleUdpTransfer(NodeContainer nodes, uint16_t port)
{
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      for (uint32_t j = 0; j < nodes.GetN(); ++j)
        {
          if (i != j)
            {
              Ptr<Node> sender = nodes.Get(i);
              Ptr<Node> receiver = nodes.Get(j);
              Ipv4Address receiverIp = receiver->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();

              uint32_t seqNum = (i * nodes.GetN()) + j;
              Time startTime = Seconds(UniformRandomVariable().GetValue(1.0, 29.0));

              Simulator::Schedule(startTime, [this, sender, receiverIp, port, seqNum]() {
                SocketFdFactory fdFactory;
                Ptr<Socket> udpSocket = Socket::CreateSocket(sender, TypeId::LookupByName("ns3::UdpSocketFactory"));
                udpSocket->Connect(InetSocketAddress(receiverIp, port));

                Packet packet;
                packet.Write(&seqNum, sizeof(seqNum));
                udpSocket->Send(&packet, 1, 0);

                this->RecordTransmission(sender->GetId(), seqNum, Simulator::Now());

                NS_LOG_DEBUG("Node " << sender->GetId() << " sent packet to " << receiverIp << " at " << Simulator::Now().GetSeconds());
              });
            }
        }
    }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));

  NodeContainer nodes;
  nodes.Create(10);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(channel.Create());

  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(500.0 / sqrt(10)),
                                "DeltaY", DoubleValue(500.0 / sqrt(10)),
                                "GridWidth", UintegerValue(ceil(sqrt(10))),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 500, 0, 500)));
  mobility.Install(nodes);

  InternetStackHelper internet;
  AodvHelper aodv;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  uint16_t port = 9;
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer apps;

  for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      apps.Add(sink.Install(nodes.Get(i)));
    }

  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(30.0));

  UdpTrafficHelper trafficHelper;
  trafficHelper.ScheduleUdpTransfer(nodes, port);

  wifiPhy.EnablePcapAll("aodv_adhoc_udp");

  Simulator::Stop(Seconds(30.0));
  Simulator::Schedule(Seconds(30.0), &UdpTrafficHelper::ReportMetrics, &trafficHelper);
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}