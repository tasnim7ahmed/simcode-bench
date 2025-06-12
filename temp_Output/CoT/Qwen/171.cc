#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvAdHocSimulation");

class UdpTrafficHelper {
public:
  void SetupUdpTraffic(Ptr<Node> sender, Ptr<Node> receiver, Ipv4Address receiverAddr, uint16_t port, double startTime);
  void HandlePacketSink(Ptr<const Packet> packet, const Address &from);
  std::vector<std::pair<Time, Time>> m_sendTimes;
  std::map<uint32_t, std::pair<Time, bool>> m_sentPackets;
  uint32_t m_packetsReceived = 0;
  uint32_t m_packetsSent = 0;
};

void UdpTrafficHelper::SetupUdpTraffic(Ptr<Node> sender, Ptr<Node> receiver, Ipv4Address receiverAddr, uint16_t port, double startTime)
{
  // Create UDP server (receiver)
  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(receiver);
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(30.0));

  // Connect the packet sink trace to capture received packets
  Config::Connect("/NodeList/" + std::to_string(receiver->GetId()) + "/ApplicationList/0/$ns3::PacketSink/Rx",
                  MakeCallback(&UdpTrafficHelper::HandlePacketSink, this));

  // Create UDP client (sender)
  OnOffHelper client("ns3::UdpSocketFactory", InetSocketAddress(receiverAddr, port));
  client.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
  client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  client.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApp = client.Install(sender);
  clientApp.Start(Seconds(startTime));
  clientApp.Stop(Seconds(30.0));

  // Schedule tracking of sent packets
  for (auto app : clientApp)
    {
      auto socket = DynamicCast<Socket>(app->GetObject<Socket>());
      if (socket)
        {
          socket->TraceConnectWithoutContext("Tx", MakeCallback(
            [this, senderId = sender->GetId()](Ptr<const Packet> p, const Address&) {
              uint32_t uid = p->GetUid();
              m_sentPackets[uid] = {Simulator::Now(), false};
              m_packetsSent++;
            }));
        }
    }
}

void UdpTrafficHelper::HandlePacketSink(Ptr<const Packet> packet, const Address &from)
{
  uint32_t uid = packet->GetUid();
  auto it = m_sentPackets.find(uid);
  if (it != m_sentPackets.end())
    {
      Time rxTime = Simulator::Now();
      Time txTime = it->second.first;
      NS_LOG_DEBUG("Packet UID=" << uid << " delay: " << rxTime - txTime);
      m_packetsReceived++;
      it->second.second = true; // Mark as received
    }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  RngSeedManager::SetSeed(12345);
  RngSeedManager::SetRun(7);

  NodeContainer nodes;
  nodes.Create(10);

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(100.0),
                                "DeltaY", DoubleValue(100.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 500, 0, 500)));
  mobility.Install(nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpTrafficHelper trafficHelper;

  // Generate random traffic between nodes
  Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
  for (uint32_t i = 0; i < 10; ++i)
    {
      for (uint32_t j = 0; j < 10; ++j)
        {
          if (i != j)
            {
              double start = rand->GetValue(1.0, 25.0); // Random start time
              trafficHelper.SetupUdpTraffic(nodes.Get(i), nodes.Get(j), interfaces.GetAddress(j, 0), 9, start);
            }
        }
    }

  // Enable pcap tracing
  wifiPhy.EnablePcapAll("aodv_adhoc_simulation");

  Simulator::Stop(Seconds(30.0));
  Simulator::Run();

  // Calculate and display metrics
  double pdr = (trafficHelper.m_packetsSent > 0)
                   ? static_cast<double>(trafficHelper.m_packetsReceived) / trafficHelper.m_packetsSent
                   : 0.0;
  std::cout << "Packet Delivery Ratio: " << pdr << std::endl;

  double totalDelay = 0.0;
  uint32_t count = 0;
  for (const auto& pkt : trafficHelper.m_sentPackets)
    {
      if (pkt.second.second) // Only received packets
        {
          totalDelay += (Simulator::Now() - pkt.second.first).GetSeconds();
          count++;
        }
    }
  double avgDelay = (count > 0) ? totalDelay / count : 0.0;
  std::cout << "Average End-to-End Delay: " << avgDelay << " seconds" << std::endl;

  Simulator::Destroy();
  return 0;
}