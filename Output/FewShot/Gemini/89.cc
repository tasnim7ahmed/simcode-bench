#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"
#include "ns3/netanim-module.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiEnergySimulation");

double GetRandomHarvestedPower() {
  static std::default_random_engine generator;
  static std::uniform_real_distribution<double> distribution(0.0, 0.1);
  return distribution(generator);
}

void PrintEnergyStats(Ptr<Node> node, std::ostream &os) {
  Ptr<BasicEnergySource> energySource = DynamicCast<BasicEnergySource>(node->GetObject<EnergySource>());
  if (energySource) {
    os << "Node " << node->GetId() << ": " << std::endl;
    os << "  Remaining Energy: " << energySource->GetRemainingEnergy() << " Joules" << std::endl;
    os << "  Total Energy Consumption: " << energySource->GetTotalEnergyConsumption() << " Joules" << std::endl;
  } else {
    os << "Energy source not found for Node " << node->GetId() << std::endl;
  }
}

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Create();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("wifi-energy");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(0));

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(1));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0), "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(10.0), "GridWidth", UintegerValue(3), "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer::Create(staDevices, apDevices));

  uint16_t port = 9;
  UdpClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set("InitialEnergyJoule", DoubleValue(1.0));

  EnergyHarvesterHelper basicHarvesterHelper;
  basicHarvesterHelper.Set("HarvestedPower", StringValue("ns3::DeterministicRandomVariable[Min=0.0|Max=0.1]"));

  Ptr<Node> node0 = nodes.Get(0);
  Ptr<Node> node1 = nodes.Get(1);

  basicSourceHelper.Install(node0);
  basicSourceHelper.Install(node1);

  basicHarvesterHelper.Install(node0->GetAggregateIterator());
  basicHarvesterHelper.Install(node1->GetAggregateIterator());

  Ptr<WifiRadioEnergyModel> radioModel0 = CreateObject<WifiRadioEnergyModel>();
  Ptr<WifiRadioEnergyModel> radioModel1 = CreateObject<WifiRadioEnergyModel>();

  radioModel0->SetNode(node0);
  radioModel1->SetNode(node1);

  radioModel0->SetAttribute("TxCurrentA", DoubleValue(0.0174));
  radioModel0->SetAttribute("RxCurrentA", DoubleValue(0.0197));
  radioModel1->SetAttribute("TxCurrentA", DoubleValue(0.0174));
  radioModel1->SetAttribute("RxCurrentA", DoubleValue(0.0197));

  node0->AggregateObject(radioModel0);
  node1->AggregateObject(radioModel1);
  
  Simulator::Schedule (Seconds(1.0), [](){
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/EnergyModel/IdleCurrentA", DoubleValue(0.0001));
  });

  std::ofstream os("energy-stats.txt");
  Simulator::Stop(Seconds(10.0));

  Simulator::Run();

  PrintEnergyStats(nodes.Get(1), os);

  Simulator::Destroy();
  os.close();

  return 0;
}