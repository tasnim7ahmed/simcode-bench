#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include <vector>
#include <utility>
#include <random>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvPdrE2eDelay");

struct FlowStats
{
  uint32_t txPackets = 0;
  uint32_t rxPackets = 0;
  double   delaySum = 0.0;
  uint32_t rxCount = 0;
};

std::map<uint64_t, FlowStats> flowStats;
std::map<uint64_t, Time> firstTxTime;

void
RxPacketCallback(Ptr<const Packet> packet, const Address &addr, uint16_t port, const Address &from, uint16_t fromPort)
{
  // Nothing to do here for now
}

void
UdpTxTrace(Ptr<const Packet> packet)
{
  uint64_t uid = packet->GetUid();
  firstTxTime[uid] = Simulator::Now();
  flowStats[uid].txPackets++;
}

void
UdpRxTrace(Ptr<const Packet> packet, const Address &address)
{
  uint64_t uid = packet->GetUid();
  flowStats[uid].rxPackets++;
  Time txTime = firstTxTime.count(uid) ? firstTxTime[uid] : Seconds(0.0);
  Time delay = Simulator::Now() - txTime;
  flowStats[uid].delaySum += delay.GetSeconds();
  flowStats[uid].rxCount++;
}

int
main(int argc, char *argv[])
{
  uint32_t numNodes = 10;
  double simTime = 30.0;
  double areaSize = 500.0;
  uint32_t port = 9;

  CommandLine cmd;
  cmd.AddValue("numNodes", "Number of nodes in the network", numNodes);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(numNodes);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                               "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                               "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
      "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=10.0]"),
      "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"),
      "PositionAllocator", PointerValue(mobility.GetPositionAllocator()));
  mobility.Install(nodes);

  InternetStackHelper stack;
  AodvHelper aodv;
  stack.SetRoutingHelper(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint32_t numFlows = numNodes;
  double appStartMin = 1.0;
  double appStartMax = 5.0;
  double appStopTime = simTime - 1.0;
  uint32_t packetSize = 512;
  double dataRateMbps = 0.5;
  uint32_t packetsPerFlow = 100;

  std::default_random_engine rng(std::random_device{}());
  std::uniform_int_distribution<uint32_t> nodeDist(0, numNodes-1);
  std::uniform_real_distribution<double> timeDist(appStartMin, appStartMax);

  std::vector<std::pair<uint32_t, uint32_t>> srcDstPairs;

  // Unique src-dst pairs
  while (srcDstPairs.size() < numFlows)
    {
      uint32_t src = nodeDist(rng);
      uint32_t dst = nodeDist(rng);
      if (src != dst)
        {
          bool exists = false;
          for (auto &pr : srcDstPairs)
            if (pr.first == src && pr.second == dst)
              exists = true;
          if (!exists)
            srcDstPairs.push_back({src, dst});
        }
    }

  for (uint32_t i = 0; i < srcDstPairs.size(); ++i)
    {
      uint32_t src = srcDstPairs[i].first;
      uint32_t dst = srcDstPairs[i].second;

      double startTime = timeDist(rng);

      // Set up receiver
      UdpServerHelper udpServer(port + i);
      ApplicationContainer serverApps = udpServer.Install(nodes.Get(dst));
      serverApps.Start(Seconds(0.0));
      serverApps.Stop(Seconds(appStopTime));

      // Set up sender
      UdpClientHelper udpClient(interfaces.GetAddress(dst), port + i);
      udpClient.SetAttribute("MaxPackets", UintegerValue(packetsPerFlow));
      udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0 / (dataRateMbps * 1e6 / (packetSize * 8)))));
      udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
      ApplicationContainer clientApps = udpClient.Install(nodes.Get(src));
      clientApps.Start(Seconds(startTime));
      clientApps.Stop(Seconds(appStopTime));

      Ptr<NetDevice> srcDev = devices.Get(src);
      Ptr<NetDevice> dstDev = devices.Get(dst);

      Ptr<UdpClient> udpClientPtr = DynamicCast<UdpClient>(clientApps.Get(0));
      Ptr<UdpServer> udpServerPtr = DynamicCast<UdpServer>(serverApps.Get(0));

      // Attach traces
      devices.Get(src)->TraceConnectWithoutContext("PhyTxBegin", MakeCallback(&UdpTxTrace));
      udpServerPtr->TraceConnectWithoutContext("Rx", MakeCallback(&UdpRxTrace));
    }

  wifiPhy.EnablePcap("adhoc-aodv", devices, true);

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  uint32_t totalTxPackets = 0;
  uint32_t totalRxPackets = 0;
  double totalDelay = 0.0;
  uint32_t totalRx = 0;
  for (const auto &it : flowStats)
    {
      totalTxPackets += it.second.txPackets;
      totalRxPackets += it.second.rxPackets;
      totalDelay += it.second.delaySum;
      totalRx += it.second.rxCount;
    }
  double pdr = (totalTxPackets > 0) ? (double)totalRxPackets / totalTxPackets * 100.0 : 0.0;
  double avgDelay = (totalRx > 0) ? totalDelay / totalRx : 0.0;

  std::cout << "Packet Delivery Ratio (PDR): " << pdr << " %\n";
  std::cout << "Average End-to-End Delay: " << avgDelay << " s\n";

  Simulator::Destroy();
  return 0;
}