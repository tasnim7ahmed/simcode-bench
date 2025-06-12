#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink-helper.h"
#include <ns3/packet.h>
#include <ns3/event-id.h>
#include <ns3/simulator.h>
#include <ns3/nstime.h>
#include <ns3/command-line.h>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvWirelessSimulation");

class SimulationHelper {
public:
  static void ReceivePacket(Ptr<const Packet> packet, const Address &from) {
    m_packetsReceived++;
    m_endToEndDelay += (Simulator::Now() - m_packetSentTimes[packet->GetUid()]);
  }

  static void SendPacket(Ptr<Socket> socket, Ipv4Address destAddr, uint16_t port) {
    Ptr<Packet> packet = Create<Packet>(1024); // 1024-byte packet
    socket->SendTo(packet, 0, InetSocketAddress(destAddr, port));
    m_packetsSent++;
    m_packetSentTimes[packet->GetUid()] = Simulator::Now();
  }

  static void ScheduleTraffic(Ptr<Socket> socket, Ipv4Address destAddr, uint16_t port, double minInterval, double maxInterval) {
    double interval = minInterval + (maxInterval - minInterval) * (rand() / (double) RAND_MAX);
    Simulator::Schedule(Seconds(interval), &SimulationHelper::ScheduleTraffic, socket, destAddr, destAddr, port, minInterval, maxInterval);
    SendPacket(socket, destAddr, port);
  }

  static uint32_t m_packetsSent;
  static uint32_t m_packetsReceived;
  static Time m_endToEndDelay;
  static std::map<uint64_t, Time> m_packetSentTimes;
};

uint32_t SimulationHelper::m_packetsSent = 0;
uint32_t SimulationHelper::m_packetsReceived = 0;
Time SimulationHelper::m_endToEndDelay = Seconds(0.0);
std::map<uint64_t, Time> SimulationHelper::m_packetSentTimes;

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  RngSeedManager::SetSeed(12345);
  RngSeedManager::SetRun(7);

  NodeContainer nodes;
  nodes.Create(10);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

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
                                "DeltaX", DoubleValue(100.0),
                                "DeltaY", DoubleValue(100.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 500, 0, 500)));
  mobility.Install(nodes);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  ApplicationContainer apps;
  uint16_t port = 9;

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    for (uint32_t j = 0; j < nodes.GetN(); ++j) {
      if (i != j) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Socket> socket = Socket::CreateSocket(node, tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
        socket->Bind(local);
        socket->SetRecvCallback(MakeCallback(&SimulationHelper::ReceivePacket));
        socket->ShutdownSend();

        apps.Add(socket->GetApplication(0));
      }
    }
  }

  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(30.0));

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    for (uint32_t j = 0; j < nodes.GetN(); ++j) {
      if (i != j) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Socket> socket = Socket::CreateSocket(node, tid);
        InetSocketAddress remote = InetSocketAddress(interfaces.GetAddress(j), port);
        socket->Connect(remote);
        socket->SetAllowBroadcast(true);

        Simulator::Schedule(Seconds(1.0 + 29.0 * (rand() / (double) RAND_MAX)),
                            &SimulationHelper::ScheduleTraffic,
                            socket, interfaces.GetAddress(j), port, 0.5, 2.0);
      }
    }
  }

  wifiPhy.EnablePcapAll("aodv_wireless_simulation");

  Simulator::Stop(Seconds(30.0));
  Simulator::Run();

  double pdr = (m_packetsSent > 0) ? (static_cast<double>(m_packetsReceived) / m_packetsSent) : 0.0;
  double avgDelay = (m_packetsReceived > 0) ? (m_endToEndDelay.GetSeconds() / m_packetsReceived) : 0.0;

  std::cout << "Packet Delivery Ratio: " << pdr << std::endl;
  std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;

  Simulator::Destroy();
  return 0;
}