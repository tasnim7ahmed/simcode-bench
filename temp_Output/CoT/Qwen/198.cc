#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("V2VWaveSimulation");

class V2VWaveStats
{
public:
  V2VWaveStats();
  void TxCallback(std::string context, Ptr<const Packet> packet);
  void RxCallback(std::string context, Ptr<const Packet> packet);
  uint32_t GetTxPackets() const;
  uint32_t GetRxPackets() const;

private:
  uint32_t m_txPackets;
  uint32_t m_rxPackets;
};

V2VWaveStats::V2VWaveStats()
  : m_txPackets(0), m_rxPackets(0)
{
}

void
V2VWaveStats::TxCallback(std::string context, Ptr<const Packet> packet)
{
  m_txPackets++;
}

void
V2VWaveStats::RxCallback(std::string context, Ptr<const Packet> packet)
{
  m_rxPackets++;
}

uint32_t
V2VWaveStats::GetTxPackets() const
{
  return m_txPackets;
}

uint32_t
V2VWaveStats::GetRxPackets() const
{
  return m_rxPackets;
}

int
main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  double simDuration = 10.0;
  double dataRate = 2; // Mbps
  double packetSize = 1024; // bytes
  double interval = 1.0; // seconds

  NodeContainer nodes;
  nodes.Create(2);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes);

  for (NodeContainer::Iterator it = nodes.Begin(); it != nodes.End(); ++it)
  {
    Ptr<Node> node = *it;
    Ptr<MobilityModel> model = node->GetObject<MobilityModel>();
    model->SetPosition(Vector(0.0, 0.0, 0.0));
    if (it == nodes.Begin())
    {
      model->SetVelocity(Vector(20.0, 0.0, 0.0)); // Moving east at 20 m/s
    }
    else
    {
      model->SetVelocity(Vector(15.0, 0.0, 0.0)); // Slower vehicle ahead
    }
  }

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  Wifi80211pMacHelper mac;
  Wifi80211pHelper wifi80211p;
  NetDeviceContainer devices = wifi80211p.Install(phy, mac, nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  UdpServerHelper server(9);
  ApplicationContainer serverApp = server.Install(nodes.Get(1));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(simDuration));

  UdpClientHelper client(interfaces.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(UINT32_MAX));
  client.SetAttribute("Interval", TimeValue(Seconds(interval)));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(simDuration - 1.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  V2VWaveStats stats;
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpClient/Tx", MakeCallback(&V2VWaveStats::TxCallback, &stats));
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&V2VWaveStats::RxCallback, &stats));

  Simulator::Stop(Seconds(simDuration));
  Simulator::Run();
  Simulator::Destroy();

  std::cout << "Sent packets: " << stats.GetTxPackets() << std::endl;
  std::cout << "Received packets: " << stats.GetRxPackets() << std::endl;

  return 0;
}