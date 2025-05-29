#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("V2VWaveSimulation");

class Stats
{
public:
  Stats() : m_txPackets(0), m_rxPackets(0) {}

  void TxCb(Ptr<const Packet> p)
  {
    m_txPackets++;
  }

  void RxCb(Ptr<const Packet> p, const Address &addr)
  {
    m_rxPackets++;
  }

  uint32_t m_txPackets;
  uint32_t m_rxPackets;
};

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("V2VWaveSimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
  posAlloc->Add(Vector(0.0, 0.0, 0.0));   // Vehicle 1 at origin
  posAlloc->Add(Vector(50.0, 0.0, 0.0));  // Vehicle 2 at (50,0,0)
  mobility.SetPositionAllocator(posAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Wave (802.11p) Configuration
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());
  QosWaveMacHelper wifi80211pMac = QosWaveMacHelper::Default();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
  wifi80211p.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                                     "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));
  NetDeviceContainer devices = wifi80211p.Install(wifiPhy, wifi80211pMac, nodes);

  // Internet stack + IP assignment
  InternetStackHelper internet;
  internet.Install(nodes);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(devices);

  // UDP Application
  uint16_t port = 4000;

  // Receiver on node 1 (nodes.Get(1))
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(nodes.Get(1));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Sender on node 0 (nodes.Get(0))
  uint32_t packetSize = 200;
  uint32_t numPackets = 50;
  double interval = 0.1; // seconds
  UdpClientHelper client(i.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(numPackets));
  client.SetAttribute("Interval", TimeValue(Seconds(interval)));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  // Statistics tracking
  Stats stats;

  // Connect packet transmission on node 0
  Ptr<NetDevice> dev0 = devices.Get(0);
  dev0->TraceConnectWithoutContext("PhyTxBegin",
    MakeCallback([&](Ptr<const Packet> p){
      stats.TxCb(p);
    })
  );

  // Connect packet reception on node 1
  Ptr<NetDevice> dev1 = devices.Get(1);
  dev1->TraceConnectWithoutContext("PhyRxEnd",
    MakeCallback([&](Ptr<const Packet> p){
      stats.RxCb(p, Address());
    })
  );

  // Enable PCAP tracing (optional)
  wifiPhy.EnablePcap("v2v-wave-vehicle0", devices.Get(0), true);
  wifiPhy.EnablePcap("v2v-wave-vehicle1", devices.Get(1), true);

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  NS_LOG_INFO("Packets transmitted: " << stats.m_txPackets);
  NS_LOG_INFO("Packets received: " << stats.m_rxPackets);

  Simulator::Destroy();
  return 0;
}