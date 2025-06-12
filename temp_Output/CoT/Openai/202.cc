#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WSN-Circular-LrWpan");

struct PacketStats
{
  uint32_t sent = 0;
  uint32_t received = 0;
  uint64_t totalDelay = 0; // in nanoseconds
  Time startTime = Seconds(0);
  Time lastPacketTime = Seconds(0);
};

PacketStats stats;

std::map<uint64_t, Time> sentTimes;

void TxTrace(Ptr<const Packet> packet)
{
  stats.sent++;
  sentTimes[packet->GetUid()] = Simulator::Now();
  if (stats.sent == 1)
    stats.startTime = Simulator::Now();
}
void RxTrace(Ptr<const Packet> packet, const Address &addr)
{
  stats.received++;
  Time sent = sentTimes.count(packet->GetUid()) ? sentTimes[packet->GetUid()] : Seconds(0);
  Time delay = Simulator::Now() - sent;
  stats.totalDelay += delay.GetNanoSeconds();
  stats.lastPacketTime = Simulator::Now();
}

int main(int argc, char *argv[])
{
  uint32_t nNodes = 6;
  double radius = 20.0; // meters
  double simTime = 30.0; // seconds
  double interval = 2.0; // seconds
  uint16_t udpPort = 60000;
  uint32_t packetSize = 40; // bytes

  CommandLine cmd;
  cmd.AddValue("simTime", "Simulation duration in seconds", simTime);
  cmd.AddValue("interval", "Packet interval", interval);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(nNodes);

  // Install Internet stacks
  InternetStackHelper internetv6;
  internetv6.Install(nodes);

  // Mobility: Circle
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> alloc = CreateObject<ListPositionAllocator>();
  for (uint32_t i = 0; i < nNodes; ++i)
  {
    double angle = 2 * M_PI * i / nNodes;
    double x = radius * std::cos(angle);
    double y = radius * std::sin(angle);
    alloc->Add(Vector(x, y, 0.0));
  }
  mobility.SetPositionAllocator(alloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // LR-WPAN
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(nodes);
  lrWpanHelper.AssociateToPan(lrwpanDevices, 0);

  // 802.15.4 PHY/Channel configured for 2.4GHz (Channel 16 is default, 2.4 GHz)
  lrWpanHelper.EnablePcapAll(std::string("wsn-circular"));

  // Assign MAC 64-bit address -> IPv6
  SixLowPanHelper sixlowpan;                                  // 6LoWPAN
  NetDeviceContainer sixlowpanDevices = sixlowpan.Install(lrwpanDevices);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer ifaces = ipv6.Assign(sixlowpanDevices);

  // Set all interfaces to be up
  for (uint32_t i = 0; i < nNodes; ++i)
  {
    ifaces.SetForwarding(i, true);
    ifaces.SetDefaultRouteInAllNodes(i);
  }

  // UDP Server (sink) on node 0
  UdpServerHelper udpServer(udpPort);
  ApplicationContainer serverApps = udpServer.Install(nodes.Get(0));
  serverApps.Start(Seconds(0.5));
  serverApps.Stop(Seconds(simTime));

  // UDP Client (source) on nodes 1..5
  for (uint32_t i = 1; i < nNodes; ++i)
  {
    UdpClientHelper udpClient(ifaces.GetAddress(0, 1), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = udpClient.Install(nodes.Get(i));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));
  }

  // Tracing for stats
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::UdpClient/Tx", MakeCallback(&TxTrace));
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&RxTrace));

  // NetAnim Setup
  AnimationInterface anim("wsn-circular.xml");
  anim.SetMaxPktsPerTraceFile(50000);
  for (uint32_t i = 0; i < nNodes; ++i)
  {
    anim.UpdateNodeDescription(i, i == 0 ? "Sink" : "Sensor");
    anim.UpdateNodeColor(i, i == 0 ? 255 : 60, i == 0 ? 0 : 180, i == 0 ? 0 : 60);
  }

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  double pdr = stats.sent ? double(stats.received) / double(stats.sent) : 0.0;
  double avgDelayMs = stats.received ? double(stats.totalDelay) / stats.received / 1e6 : 0.0;
  double throughputKbps = (stats.received * packetSize * 8.0) / (stats.lastPacketTime.GetSeconds() - stats.startTime.GetSeconds() + 1e-6) / 1024.0;

  std::cout << "Packets Sent:     " << stats.sent << std::endl;
  std::cout << "Packets Received: " << stats.received << std::endl;
  std::cout << "PDR:              " << pdr * 100 << " %" << std::endl;
  std::cout << "Avg Delay:        " << avgDelayMs << " ms" << std::endl;
  std::cout << "Throughput:       " << throughputKbps << " kbps" << std::endl;

  Simulator::Destroy();
  return 0;
}