#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnergyHarvestingSimulation");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("EnergyHarvestingSimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Create();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(1));

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX",
                                DoubleValue(0.0), "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0), "DeltaY",
                                DoubleValue(10.0), "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign(staDevices);
  interfaces = address.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(11.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(12.0));

  // Energy Model
  BasicEnergySourceHelper basicSourceHelper;
  EnergySourceContainer sources = basicSourceHelper.Install(nodes);
  for (uint32_t i = 0; i < sources.GetN(); ++i) {
      Ptr<BasicEnergySource> basicSourcePtr = DynamicCast<BasicEnergySource>(sources.Get(i));
      basicSourcePtr->SetInitialEnergy(1); // Joules
  }

  WifiRadioEnergyModelHelper radioEnergyModelHelper;
  radioEnergyModelHelper.Set("TxCurrentA", DoubleValue(0.0174));
  radioEnergyModelHelper.Set("RxCurrentA", DoubleValue(0.0197));
  EnergyModelContainer deviceModels = radioEnergyModelHelper.Install(staDevices.Get(0), sources.Get(1));
  deviceModels.Add(radioEnergyModelHelper.Install(apDevices.Get(0), sources.Get(0)));

  BasicEnergyHarvesterHelper basicHarvesterHelper;
  basicHarvesterHelper.Set("HarvestedPower", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=0.1]"));
  EnergyHarvesterContainer harvesters = basicHarvesterHelper.Install(sources);

  Simulator::Schedule(Seconds(0.0), []() {
      NS_LOG_INFO("Starting Simulation");
  });

  Simulator::Schedule(Seconds(11.0), []() {
    Ptr<Node> node = NodeList::GetNode(1);
    Ptr<EnergySource> source = node->GetObject<EnergySource>();
    NS_ASSERT (source != NULL);
    NS_LOG_INFO("Energy Statistics at time 11s:");
    NS_LOG_INFO("  Remaining energy: " << source->GetRemainingEnergy() << " J");
    NS_LOG_INFO("  Total energy consumption: " << source->GetTotalEnergyConsumption() << " J");

    Ptr<BasicEnergyHarvester> harvester = DynamicCast<BasicEnergyHarvester>(EnergyHarvesterContainer(source).Get(0));
    NS_ASSERT(harvester != NULL);

    NS_LOG_INFO("  Total harvested energy: " << harvester->GetTotalEnergyHarvested() << " J");

  });

  Simulator::Stop(Seconds(12.0));

  AsciiTraceHelper ascii;
  phy.EnableAsciiAll(ascii.CreateFileStream("energy-simulation.tr"));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}