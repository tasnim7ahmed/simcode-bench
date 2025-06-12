#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/command-line.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SensorNetworkSimulation");

int main(int argc, char *argv[]) {
  // Enable logging
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable("OlsrRoutingProtocol", LOG_LEVEL_ALL); // Enable OLSR logging for debugging

  // Set simulation time
  double simulationTime = 50.0;

  // Number of sensor nodes
  uint32_t numNodes = 10;

  // Packet size and interval
  uint32_t packetSize = 1024;
  double packetInterval = 1.0; // Send a packet every 1 second

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("numNodes", "Number of sensor nodes", numNodes);
  cmd.AddValue("packetSize", "Size of packets sent", packetSize);
  cmd.AddValue("packetInterval", "Interval between packet sends", packetInterval);
  cmd.Parse(argc, argv);

  // Create nodes
  NodeContainer sensorNodes;
  sensorNodes.Create(numNodes);
  NodeContainer sinkNode;
  sinkNode.Create(1);

  NodeContainer allNodes;
  allNodes.Add(sensorNodes);
  allNodes.Add(sinkNode);

  // Install wireless devices
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer sensorDevices = wifi.Install(wifiPhy, wifiMac, sensorNodes);
  NetDeviceContainer sinkDevice = wifi.Install(wifiPhy, wifiMac, sinkNode);

  // Install OLSR routing
  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper(olsr);
  internet.Install(allNodes);

  // Assign addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sensorInterfaces = ipv4.Assign(sensorDevices);
  Ipv4InterfaceContainer sinkInterface = ipv4.Assign(sinkDevice);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("50.0"),
                                "Y", StringValue("50.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=20.0]"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(allNodes);

  // Install applications (UDP client on sensors, UDP server on sink)
  uint16_t port = 9; // Discard port (RFC 863)

  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(sinkNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime - 1));

  UdpClientHelper client(sinkInterface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numNodes; ++i) {
    clientApps.Add(client.Install(sensorNodes.Get(i)));
  }

  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime - 2));

  // Energy Model
  BasicEnergySourceHelper basicSourceHelper;
  EnergySourceContainer sources = basicSourceHelper.Install(allNodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.0174)); // 17.4mA
  radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0197)); // 19.7mA
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(sensorDevices);
  deviceModels.Add(radioEnergyHelper.Install(sinkDevice));

  // Connect energy source to radio energy device
  for (uint32_t i = 0; i < allNodes.GetN(); ++i)
  {
     Ptr<Node> node = allNodes.Get(i);
     Ptr<EnergySource> source = sources.Get(i);
     Ptr<DeviceEnergyModel> device = (i < numNodes) ? deviceModels.Get(i) : deviceModels.Get(numNodes); // sink node uses the last device model
     source->SetNode(node);
     if (device)
     {
        device->SetNode(node);
        source->AppendDeviceEnergyModel(device);
     }
  }

  // Run simulation
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}