#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiEnergyConsumptionExample");

void
RemainingEnergyTrace(double oldValue, double newValue)
{
  NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s Remaining energy: " << newValue << " J");
}

void
TotalEnergyConsumptionTrace(double oldValue, double newValue)
{
  NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s Total energy consumed: " << newValue << " J");
}

int
main(int argc, char *argv[])
{
  double simTime = 10.0;
  double nodeDistance = 20.0;
  uint32_t packetSize = 1024;
  double startTime = 1.0;

  CommandLine cmd;
  cmd.AddValue("packetSize", "Size of UDP packets in bytes", packetSize);
  cmd.AddValue("startTime", "Application start time", startTime);
  cmd.AddValue("nodeDistance", "Distance between the two nodes (meters)", nodeDistance);
  cmd.AddValue("simTime", "Total simulation time (seconds)", simTime);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(nodeDistance, 0.0, 0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Wi-Fi configuration
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  WifiMacHelper wifiMac;
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());
  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Energy Model
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10.0));
  EnergySourceContainer sources = energySourceHelper.Install(nodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, sources);

  // Trace remaining energy and total energy consumption
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      Ptr<EnergySource> src = sources.Get(i);
      src->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergyTrace));
      Ptr<DeviceEnergyModel> devModel = deviceModels.Get(i);
      devModel->TraceConnectWithoutContext("TotalEnergyConsumption", MakeCallback(&TotalEnergyConsumptionTrace));
    }

  // Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // IP assignment
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces = address.Assign(devices);

  // UDP application
  uint16_t port = 9;

  // Receiver on node 1
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(nodes.Get(1));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(simTime));

  // Sender on node 0
  UdpClientHelper client(ifaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(100));
  client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(startTime));
  clientApp.Stop(Seconds(simTime));

  // Log packet transmissions
  Config::Connect("/NodeList/0/ApplicationList/*/$ns3::UdpClient/Tx",
                  MakeCallback([](Ptr<const Packet> p) {
                      NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s Tx packet of size " << p->GetSize());
                  }));

  Config::Connect("/NodeList/1/ApplicationList/*/$ns3::UdpServer/Rx",
                  MakeCallback([](Ptr<const Packet> p, const Address &addr) {
                      NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s Rx packet of size " << p->GetSize());
                  }));

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  // Verify consumed energy within bounds (e.g., nodes should not have energy < 0)
  for (uint32_t i = 0; i < sources.GetN(); ++i)
    {
      double rem = sources.Get(i)->GetRemainingEnergy();
      NS_LOG_UNCOND("Node " << i << " remaining energy: " << rem << " J");
      if (rem < 0.0 || rem > 10.0)
        {
          NS_LOG_ERROR("Energy for node " << i << " out of bounds!");
        }
    }

  Simulator::Destroy();
  return 0;
}