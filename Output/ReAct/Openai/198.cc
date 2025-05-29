#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("V2VWaveSimulation");

class AppStats
{
public:
  uint32_t sent;
  uint32_t received;
  AppStats() : sent(0), received(0) {}
};

AppStats stats;

void TxTrace(Ptr<const Packet> p)
{
  stats.sent++;
}

void RxTrace(Ptr<const Packet> p, const Address &addr)
{
  stats.received++;
}

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  // Create two vehicle nodes
  NodeContainer vehicles;
  vehicles.Create(2);

  // Configure mobility (static, separate positions)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
  pos->Add(Vector(0.0, 0.0, 0.0));    // Vehicle 1
  pos->Add(Vector(50.0, 0.0, 0.0));   // Vehicle 2 (50 meters away)
  mobility.SetPositionAllocator(pos);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(vehicles);

  // Install 802.11p/WAVE devices
  YansWavePhyHelper wavePhy = YansWavePhyHelper::Default();
  YansWaveChannelHelper waveChannel = YansWaveChannelHelper::Default();
  wavePhy.SetChannel(waveChannel.Create());

  QosWaveMacHelper waveMac = QosWaveMacHelper::Default();
  WaveHelper waveHelper = WaveHelper::Default();

  NetDeviceContainer devices = waveHelper.Install(wavePhy, waveMac, vehicles);

  // Install internet stack
  InternetStackHelper internet;
  internet.Install(vehicles);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // Create UDP application: sender on node 0, receiver on node 1
  uint16_t port = 4000;
  // Receiver (PacketSink)
  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sinkHelper.Install(vehicles.Get(1));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  // Sender (OnOffApplication)
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  onoff.SetAttribute("DataRate", StringValue("1Mbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(512));
  onoff.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
  onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
  ApplicationContainer senderApp = onoff.Install(vehicles.Get(0));

  // Trace transmissions (at sender socket)
  Ptr<OnOffApplication> app = DynamicCast<OnOffApplication>(senderApp.Get(0));
  Ptr<Socket> txSocket = app->GetSocket();

  if (!txSocket)
  {
    NS_ABORT_MSG("Failed to get sender socket");
  }
  txSocket->TraceConnectWithoutContext("Tx", MakeCallback(&TxTrace));

  // Trace receptions (at sink)
  Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(0));
  sink->TraceConnectWithoutContext("Rx", MakeCallback(&RxTrace));

  // Enable logging
  LogComponentEnable("V2VWaveSimulation", LOG_LEVEL_INFO);

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  NS_LOG_INFO("Packets sent: " << stats.sent);
  NS_LOG_INFO("Packets received: " << stats.received);

  Simulator::Destroy();
  return 0;
}