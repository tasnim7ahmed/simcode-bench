#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SensorNetworkOlsrUdpSimulation");

void
RemainingEnergyTrace(double oldValue, double newValue)
{
  // Optional: Uncomment to print energy trace
  // std::cout << Simulator::Now().GetSeconds() << "s: Remaining energy = " << newValue << " J" << std::endl;
}

int
main(int argc, char *argv[])
{
  uint32_t nSensorNodes = 10;
  double simulationTime = 50.0; // seconds
  double txInterval = 5.0; // seconds
  uint16_t sinkPort = 4000;

  CommandLine cmd;
  cmd.AddValue("nSensorNodes", "Number of sensor nodes", nSensorNodes);
  cmd.AddValue("txInterval", "Transmission interval (s)", txInterval);
  cmd.Parse(argc, argv);

  NodeContainer sensorNodes;
  sensorNodes.Create(nSensorNodes);
  Ptr<Node> sinkNode = CreateObject<Node>();

  NodeContainer allNodes;
  allNodes.Add(sensorNodes);
  allNodes.Add(sinkNode);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",
                              StringValue("DsssRate11Mbps"), "ControlMode",
                              StringValue("DsssRate1Mbps"));
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  double radius = 30.0;
  double centerX = 60.0;
  double centerY = 60.0;
  double angleStep = 2.0 * M_PI / nSensorNodes;
  for (uint32_t i = 0; i < nSensorNodes; ++i)
  {
    double angle = i * angleStep;
    double x = centerX + radius * std::cos(angle);
    double y = centerY + radius * std::sin(angle);
    positionAlloc->Add(Vector(x, y, 0.0));
  }
  // Place sink at the center
  positionAlloc->Add(Vector(centerX, centerY, 0.0));

  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(allNodes);

  // Energy model setup
  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(5.0));
  EnergySourceContainer sources = basicSourceHelper.Install(sensorNodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.0174));
  radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0197));

  DeviceEnergyModelContainer deviceEnergyModels = radioEnergyHelper.Install(devices.GetN() - 1, devices.GetN() - nSensorNodes, sensors);

  // Trace remaining energy on first sensor node for demonstration
  sources.Get(0)->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergyTrace));

  // Internet and routing
  InternetStackHelper stack;
  OlsrHelper olsr;
  stack.SetRoutingHelper(olsr);
  stack.Install(allNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // UDP server (sink) on central node
  UdpServerHelper udpServer(sinkPort);
  ApplicationContainer serverApps = udpServer.Install(sinkNode);
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simulationTime));

  // UDP clients (sensors): each sensor node sends data to sink
  for (uint32_t i = 0; i < nSensorNodes; ++i)
  {
    UdpClientHelper udpClient(interfaces.GetAddress(nSensorNodes), sinkPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(txInterval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(32));
    ApplicationContainer clientApps = udpClient.Install(sensorNodes.Get(i));
    clientApps.Start(Seconds(1.0 + i * 0.1));
    clientApps.Stop(Seconds(simulationTime));
  }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}