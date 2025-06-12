#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-server.h"
#include "ns3/udp-client.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvAdHocSimulation");

class SimulationHelper {
public:
  static void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
      // Packet received, do nothing but keep track in flow monitor
    }
  }

  static Ptr<Socket> SetupPacketSink(Ptr<Node> node, Address& sinkAddress) {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> sinkSocket = Socket::CreateSocket(node, tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    sinkSocket->Bind(local);
    sinkSocket->SetRecvCallback(MakeCallback(&ReceivePacket));
    sinkAddress = InetSocketAddress(Ipv4Address("255.255.255.255"), 9);
    return sinkSocket;
  }
};

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  RngSeedManager::SetSeed(12345);
  RngSeedManager::SetRun(7);

  NodeContainer nodes;
  nodes.Create(10);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ArfRateControl");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

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

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(100));

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("aodv.tr");
  wifiPhy.EnablePcapAll("aodv_simulation");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  std::vector<Ptr<Socket>> sinks;
  std::vector<Address> sinkAddresses;

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Address sinkAddress;
    Ptr<Socket> sink = SimulationHelper::SetupPacketSink(nodes.Get(i), sinkAddress);
    sinks.push_back(sink);
    sinkAddresses.push_back(sinkAddress);
  }

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    uint32_t destIndex = i;
    while (destIndex == i) {
      destIndex = rand() % nodes.GetN();
    }
    Ptr<Socket> source = Socket::CreateSocket(nodes.Get(i), TypeId::LookupByName("ns3::UdpSocketFactory"));
    InetSocketAddress remote(interfaces.GetAddress(destIndex), 9);
    source->Connect(remote);
    Simulator::Schedule(Seconds(UniformVariable{1.0, 29.0}.GetValue()), &Socket::Send, source, MakePacketTaggerCallback(), 1024, 0);
  }

  Simulator::Stop(Seconds(30.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  double totalRx = 0, totalTx = 0, totalDelay = 0;
  for (auto it = stats.begin(); it != stats.end(); ++it) {
    auto flowStats = it->second;
    totalRx += flowStats.rxPackets;
    totalTx += flowStats.txPackets;
    totalDelay += flowStats.delaySum.ToDouble(Time::S);
  }

  double pdr = (totalTx > 0) ? (totalRx / totalTx) : 0.0;
  double avgDelay = (totalRx > 0) ? (totalDelay / totalRx) : 0.0;
  NS_LOG_UNCOND("Packet Delivery Ratio: " << pdr);
  NS_LOG_UNCOND("Average End-to-End Delay: " << avgDelay << " seconds");

  Simulator::Destroy();
  return 0;
}