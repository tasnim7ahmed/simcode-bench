#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiEnergyDemo");

void
RemainingEnergyCallback(double oldValue, double remainingEnergy)
{
  NS_LOG_UNCOND(Simulator::Now().GetSeconds()
                << "s: Remaining energy = " << remainingEnergy << " J");
}

void
TotalEnergyConsumptionCallback(double oldValue, double totalConsumed)
{
  NS_LOG_UNCOND(Simulator::Now().GetSeconds()
                << "s: Total energy consumed = " << totalConsumed << " J");
}

void
RxCallback(Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_UNCOND(Simulator::Now().GetSeconds()
                << "s: Received " << packet->GetSize() << " bytes at "
                << InetSocketAddress::ConvertFrom(address).GetIpv4());
}

int
main(int argc, char *argv[])
{
  // Default simulation parameters
  uint32_t packetSize = 1024;           // bytes
  double simulationStart = 1.0;         // seconds
  double simulationStop = 10.0;         // seconds
  double nodeDistance = 20.0;           // meters
  uint32_t nPackets = 10;               // number of packets to send
  double txInterval = 1.0;              // seconds

  // Command-line argument parsing
  CommandLine cmd;
  cmd.AddValue("packetSize", "UDP packet size (bytes)", packetSize);
  cmd.AddValue("simulationStart", "Transmission start time (s)", simulationStart);
  cmd.AddValue("simulationStop", "Simulation stop time (s)", simulationStop);
  cmd.AddValue("nodeDistance", "Distance between nodes (m)", nodeDistance);
  cmd.AddValue("nPackets", "Number of packets to send", nPackets);
  cmd.AddValue("txInterval", "Interval between packets (s)", txInterval);
  cmd.Parse(argc, argv);

  // Nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Wifi PHY and MAC helpers
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);
  WifiMacHelper mac;

  Ssid ssid = Ssid("energy-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice = wifi.Install(phy, mac, nodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, nodes.Get(1));

  NetDeviceContainer devices;
  devices.Add(staDevice.Get(0));
  devices.Add(apDevice.Get(0));

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(nodeDistance, 0.0, 0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Energy Source
  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(1.0)); // 1 Joule
  EnergySourceContainer sources = basicSourceHelper.Install(nodes);

  // Wifi Radio Energy Model
  WifiRadioEnergyModelHelper radioHelper;
  DeviceEnergyModelContainer deviceModels = radioHelper.Install(devices, sources);

  // Tracing energy
  for (uint32_t i = 0; i < deviceModels.GetN(); ++i) {
    Ptr<WifiRadioEnergyModel> model = DynamicCast<WifiRadioEnergyModel>(deviceModels.Get(i));
    model->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergyCallback));
    model->TraceConnectWithoutContext("TotalEnergyConsumption", MakeCallback(&TotalEnergyConsumptionCallback));
  }

  // Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // UDP traffic: node 0 sends to node 1
  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(nodes.Get(1));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(simulationStop + 1.0));

  UdpClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(nPackets));
  client.SetAttribute("Interval", TimeValue(Seconds(txInterval)));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApp = client.Install(nodes.Get(0));

  clientApp.Start(Seconds(simulationStart));
  clientApp.Stop(Seconds(simulationStop));

  // Trace receive packets on server
  Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(serverApp.Get(0));
  if (udpServer)
    udpServer->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));

  // Enable logging for caution (optional)
  // LogComponentEnable("WifiEnergyDemo", LOG_LEVEL_INFO);

  Simulator::Stop(Seconds(simulationStop + 2.0));
  Simulator::Run();
  Simulator::Destroy();

  // Check energy consumption limits (example: must not fully drain source)
  for (uint32_t i = 0; i < sources.GetN(); ++i) {
    Ptr<BasicEnergySource> es = DynamicCast<BasicEnergySource>(sources.Get(i));
    double finalEnergy = es->GetRemainingEnergy();
    NS_LOG_UNCOND("Node " << i << " final energy: " << finalEnergy << " J");
    if (finalEnergy < 0.01) {
      NS_LOG_UNCOND("Warning: Node " << i << " nearly depleted its energy!");
    }
  }

  return 0;
}