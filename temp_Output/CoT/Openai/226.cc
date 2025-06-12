#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WsnOlsrUdpSimulation");

int main (int argc, char *argv[])
{
  uint32_t numSensorNodes = 10;
  double simulationTime = 50.0; // seconds
  double sinkX = 50.0;
  double sinkY = 50.0;
  double areaSize = 100.0; // meters
  uint16_t udpPort = 5555;
  double packetInterval = 5.0; // seconds
  uint32_t packetSize = 40; // bytes

  CommandLine cmd;
  cmd.AddValue ("numSensorNodes", "Number of sensor nodes", numSensorNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  NodeContainer sensorNodes;
  NodeContainer sinkNode;
  sensorNodes.Create (numSensorNodes);
  sinkNode.Create (1);

  NodeContainer allNodes;
  allNodes.Add (sinkNode);
  allNodes.Add (sensorNodes);

  // WiFi PHY and channel config
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifiPhy.Set ("TxPowerStart", DoubleValue (0.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (0.0));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices;
  devices = wifi.Install (wifiPhy, wifiMac, allNodes);

  // Mobility (random in area, sink fixed)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
  posAlloc->Add (Vector (sinkX, sinkY, 0.0));
  for (uint32_t i = 0; i < numSensorNodes; ++i)
    {
      double x = areaSize * (double)std::rand () / RAND_MAX;
      double y = areaSize * (double)std::rand () / RAND_MAX;
      posAlloc->Add (Vector (x, y, 0.0));
    }
  mobility.SetPositionAllocator (posAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (allNodes);

  // Energy model (Battery source per sensor node)
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (3.0));
  EnergySourceContainer sources = energySourceHelper.Install (sensorNodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.0174)); // example values
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.0197));
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (devices.GetN() > 1 ? NetDeviceContainer (devices.Begin () + 1, devices.End ()) : devices);

  // Internet + OLSR routing
  InternetStackHelper internet;
  OlsrHelper olsr;
  internet.SetRoutingHelper (olsr);
  internet.Install (allNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // UDP Sink on the sink node
  UdpServerHelper udpServer (udpPort);
  ApplicationContainer serverApp = udpServer.Install (sinkNode.Get (0));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simulationTime));

  // UDP Client on each sensor node (send periodic packets to sink)
  for (uint32_t i = 0; i < sensorNodes.GetN (); ++i)
    {
      UdpClientHelper udpClient (interfaces.GetAddress (0), udpPort);
      udpClient.SetAttribute ("MaxPackets", UintegerValue (uint32_t (simulationTime / packetInterval)));
      udpClient.SetAttribute ("Interval", TimeValue (Seconds (packetInterval)));
      udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
      ApplicationContainer clientApps = udpClient.Install (sensorNodes.Get (i));
      clientApps.Start (Seconds (1.0 + i));
      clientApps.Stop (Seconds (simulationTime));
    }

  wifiPhy.EnablePcapAll ("wsn-olsr-udp");

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}