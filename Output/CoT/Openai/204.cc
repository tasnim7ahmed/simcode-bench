#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include <fstream>
#include <vector>
#include <numeric>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WsnCsmaGridExample");

struct PacketStats
{
  uint32_t sent = 0;
  uint32_t received = 0;
  std::vector<double> delays;
};

PacketStats stats;

std::map<uint64_t, Time> packetSentTimes;

void TxTrace(Ptr<const Packet> packet)
{
  uint64_t uid = packet->GetUid();
  packetSentTimes[uid] = Simulator::Now();
  stats.sent++;
}

void RxTrace(Ptr<const Packet> packet, const Address &)
{
  uint64_t uid = packet->GetUid();
  stats.received++;
  auto it = packetSentTimes.find(uid);
  if (it != packetSentTimes.end())
    {
      double delay = (Simulator::Now() - it->second).GetMilliSeconds();
      stats.delays.push_back(delay);
      packetSentTimes.erase(it);
    }
}

int main(int argc, char *argv[])
{
  uint32_t nNodes = 6;
  double simTime = 20.0;
  double sendInterval = 2.0; // seconds

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(nNodes);

  // Position nodes in 2x3 grid
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  double stepX = 30.0, stepY = 30.0;
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      uint32_t row = i / 3;
      uint32_t col = i % 3;
      positionAlloc->Add(Vector(10.0 + col * stepX, 10.0 + row * stepY, 0.0));
    }
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t udpPort = 9;

  // Node 0 as base station; others are sensors sending data
  ApplicationContainer sinkApp;
  UdpServerHelper udpServer(udpPort);
  sinkApp = udpServer.Install(nodes.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(simTime + 1.0));

  ApplicationContainer clientApps;

  for (uint32_t i = 1; i < nNodes; ++i)
    {
      UdpClientHelper udpClient(interfaces.GetAddress(0), udpPort);
      udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
      udpClient.SetAttribute("Interval", TimeValue(Seconds(sendInterval)));
      udpClient.SetAttribute("PacketSize", UintegerValue(50));

      ApplicationContainer client = udpClient.Install(nodes.Get(i));
      client.Start(Seconds(1.0 + i * 0.1));
      client.Stop(Seconds(simTime));
      clientApps.Add(client);
    }

  // Tracing
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::UdpClient/Tx", MakeCallback(&TxTrace));
  Config::Connect("/NodeList/0/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&RxTrace));

  AnimationInterface anim("wsn-csma-grid.xml");
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      if (i == 0)
        anim.UpdateNodeDescription(nodes.Get(i), "BaseStation");
      else
        anim.UpdateNodeDescription(nodes.Get(i), "Sensor");
    }

  // NetAnim packet flows
  anim.EnablePacketMetadata(true);

  Simulator::Stop(Seconds(simTime + 2.0));
  Simulator::Run();

  double pdr = stats.sent ? 100.0 * stats.received / stats.sent : 0.0;
  double avgDelay = stats.delays.empty() ? 0.0 : std::accumulate(stats.delays.begin(), stats.delays.end(), 0.0) / stats.delays.size();

  std::cout << "Sent packets: " << stats.sent << std::endl;
  std::cout << "Received packets: " << stats.received << std::endl;
  std::cout << "Packet Delivery Ratio: " << pdr << "%" << std::endl;
  std::cout << "Average End-to-End Delay: " << avgDelay << " ms" << std::endl;

  Simulator::Destroy();
  return 0;
}