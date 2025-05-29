#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetExample");

struct Metrics
{
  uint32_t txPackets = 0;
  uint32_t rxPackets = 0;
  std::vector<Time> sentTimes;
  std::vector<Time> recvTimes;
};

Metrics metrics;

void
TxTrace(Ptr<const Packet> packet)
{
  metrics.txPackets++;
  metrics.sentTimes.push_back(Simulator::Now());
}

void
RxTrace(Ptr<const Packet> packet, const Address &address)
{
  metrics.rxPackets++;
  metrics.recvTimes.push_back(Simulator::Now());
}

void
TrackPositions(Ptr<Node> node, uint32_t nodeId)
{
  Vector pos = node->GetObject<MobilityModel>()->GetPosition();
  NS_LOG_UNCOND("Time " << Simulator::Now().GetSeconds() << "s: Node " << nodeId
                        << " Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")");
  Simulator::Schedule(Seconds(1.0), &TrackPositions, node, nodeId);
}

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(3);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                            "Speed", StringValue("ns3::UniformRandomVariable[Min=2.0|Max=4.0]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=10|Max=90]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=10|Max=90]"));
  mobility.Install(nodes);

  // Track positions for all nodes
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
    Simulator::Schedule(Seconds(0.0), &TrackPositions, nodes.Get(i), i);

  // Wifi
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());
  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("manet-ssid");
  wifiMac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Internet stack with MANET routing (AODV)
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // UDP: one node is sender, others are receivers
  uint16_t port = 4000;
  uint32_t packetSize = 512;
  double interPacketInterval = 1.0; // seconds
  uint32_t numPackets = 10;

  // Install UDP receivers on Node 1 and 2
  for (uint32_t i = 1; i < 3; ++i)
  {
    UdpServerHelper server(port);
    ApplicationContainer apps = server.Install(nodes.Get(i));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(25.0));
  }

  // Install UDP client on Node 0
  UdpClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(numPackets));
  client.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(25.0));

  // Trace transmissions
  devices.Get(0)->TraceConnectWithoutContext("PhyTxBegin", MakeCallback(&TxTrace));
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&RxTrace));

  // NetAnim
  AnimationInterface anim("manet.xml");
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    anim.UpdateNodeDescription(nodes.Get(i), "Node" + std::to_string(i));
    anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0);
  }
  anim.EnablePacketMetadata(true);

  Simulator::Stop(Seconds(30.0));
  Simulator::Run();

  // Compute metrics
  double pdr = 0.0, avgLatency = 0.0;
  if (metrics.txPackets > 0)
    pdr = 100.0 * metrics.rxPackets / metrics.txPackets;

  uint32_t matches = std::min(metrics.sentTimes.size(), metrics.recvTimes.size());
  if (matches > 0)
  {
    Time totalDelay = Seconds(0);
    for (uint32_t i = 0; i < matches; ++i)
    {
      totalDelay += (metrics.recvTimes[i] - metrics.sentTimes[i]);
    }
    avgLatency = totalDelay.GetMilliSeconds() / matches;
  }

  std::cout << "Packets sent:    " << metrics.txPackets << std::endl;
  std::cout << "Packets received:" << metrics.rxPackets << std::endl;
  std::cout << "Packet Delivery Ratio: " << pdr << " %" << std::endl;
  std::cout << "Average Latency: " << avgLatency << " ms" << std::endl;

  Simulator::Destroy();
  return 0;
}