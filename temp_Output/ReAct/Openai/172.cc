#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <vector>
#include <numeric>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvSimulation");

struct PacketStats
{
  uint32_t rxPackets = 0;
  uint32_t txPackets = 0;
  uint32_t rxBytes = 0;
  uint32_t txBytes = 0;
  std::vector<double> delaySumMs;
};

PacketStats stats;

std::map<uint64_t, Time> packetSentTime;

void TxTrace(Ptr<const Packet> packet)
{
  // Unique identifier: Packet UID
  packetSentTime[packet->GetUid()] = Simulator::Now();
  stats.txPackets++;
  stats.txBytes += packet->GetSize();
}

void RxTrace(Ptr<const Packet> packet, const Address &from)
{
  stats.rxPackets++;
  stats.rxBytes += packet->GetSize();
  auto it = packetSentTime.find(packet->GetUid());
  if (it != packetSentTime.end())
  {
    Time delay = Simulator::Now() - it->second;
    stats.delaySumMs.push_back(delay.GetMilliSeconds());
  }
}

int main(int argc, char *argv[])
{
  uint32_t numNodes = 10;
  double simDuration = 30.0;
  double areaSize = 500.0;
  double nodeSpeed = 5.0; // max speed (m/s) for random waypoint mobility
  double pauseTime = 0.0; // No pause
  double udpRate = 512; // bits per second per flow, can be adjusted
  uint16_t portBase = 5000;

  // Create nodes
  NodeContainer nodes;
  nodes.Create(numNodes);

  // Configure WiFi (ad-hoc)
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Mobility
  MobilityHelper mobility;
  // RWP with all nodes moving within 500x500 m area
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));

  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"),
                             "PositionAllocator", PointerValue (mobility.GetPositionAllocator ()));
  mobility.Install(nodes);

  // Internet stack and AODV routing
  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(aodv);
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // UDP traffic: random pairs, random start times
  uint32_t numFlows = numNodes;
  // To make sure sources and sinks are not the same node easily
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  ApplicationContainer clientApps, serverApps;

  for(uint32_t i = 0; i < numFlows; ++i)
  {
    uint32_t src = i;
    uint32_t dst;
    do {
      dst = uv->GetInteger(0, numNodes-1);
    } while (dst == src);

    // Install UDP server on dst
    UdpServerHelper server(portBase + i);
    ApplicationContainer serverApp = server.Install(nodes.Get(dst));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simDuration));
    serverApps.Add(serverApp);

    // UDP client on src, random start between 1.0s and 5.0s
    double startTime = uv->GetValue(1.0, 5.0);
    UdpClientHelper client(interfaces.GetAddress(dst), portBase + i);
    client.SetAttribute("MaxPackets", UintegerValue(10000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApp = client.Install(nodes.Get(src));
    clientApp.Start(Seconds(startTime));
    clientApp.Stop(Seconds(simDuration));
    clientApps.Add(clientApp);
  }

  // Connect traces
  Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::UdpClient/Tx", MakeCallback(&TxTrace));
  Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&RxTrace));

  // Enable pcap tracing at PHY layer
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.EnablePcapAll("adhoc-aodv");

  // Run simulation
  Simulator::Stop (Seconds(simDuration));
  Simulator::Run ();

  // Calculate PDR and avg delay
  double pdr = 0.0;
  double avgDelay = 0.0;
  if (stats.txPackets > 0)
    pdr = (double)stats.rxPackets / stats.txPackets * 100.0;
  if (!stats.delaySumMs.empty())
    avgDelay = std::accumulate(stats.delaySumMs.begin(), stats.delaySumMs.end(), 0.0) / stats.delaySumMs.size();

  std::cout << "Simulation Results:" << std::endl;
  std::cout << "  Sent packets:     " << stats.txPackets << std::endl;
  std::cout << "  Received packets: " << stats.rxPackets << std::endl;
  std::cout << "  Packet Delivery Ratio (PDR): " << pdr << " %" << std::endl;
  std::cout << "  Average end-to-end delay:    " << avgDelay << " ms" << std::endl;

  Simulator::Destroy ();
  return 0;
}