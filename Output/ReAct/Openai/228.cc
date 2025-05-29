#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <fstream>
#include <string>
#include <sstream>

using namespace ns3;

// Helper function to load mobility from SUMO (TraCI trace or .ns2mobility file)
void
LoadSumoMobility(const std::string& filename, NodeContainer& nodes)
{
  Ns2MobilityHelper ns2 = Ns2MobilityHelper(filename);
  ns2.Install();
}

int
main(int argc, char *argv[])
{
  uint32_t numVehicles = 10;
  std::string sumoMobilityFile = "sumo-output.ns2mobility"; // You must provide this file (converted from SUMO via SUMO's trace exporters)
  double simulationTime = 80.0; // seconds

  CommandLine cmd;
  cmd.AddValue("numVehicles", "Number of vehicles in the VANET", numVehicles);
  cmd.AddValue("sumoMobilityFile", "Path to ns2mobility file exported from SUMO", sumoMobilityFile);
  cmd.Parse(argc, argv);

  NodeContainer vehicles;
  vehicles.Create(numVehicles);

  // Install realistic mobility from SUMO
  LoadSumoMobility(sumoMobilityFile, vehicles);

  // Wi-Fi PHY and MAC setup
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer vehDevs = wifi.Install(wifiPhy, wifiMac, vehicles);

  // Internet and DSR (Dynamic Source Routing) protocol install
  InternetStackHelper internet;
  DsrMainHelper dsrMain;
  DsrHelper dsr;
  internet.SetRoutingHelper(dsr);
  internet.Install(vehicles);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer vehIfs = ipv4.Assign(vehDevs);

  // UDP server (on last vehicle)
  uint16_t udpPort = 4000;
  UdpServerHelper server(udpPort);
  ApplicationContainer serverApp = server.Install(vehicles.Get(numVehicles-1));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(simulationTime));

  // UDP client (on first vehicle) - send to last node
  uint32_t maxPackets = 320;
  Time interPacketInterval = Seconds(0.25);
  UdpClientHelper client(vehIfs.GetAddress(numVehicles-1), udpPort);
  client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  client.SetAttribute("Interval", TimeValue(interPacketInterval));
  client.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApp = client.Install(vehicles.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(simulationTime-1));

  // Enable DSR logging (optional)
  // LogComponentEnable ("DsrRoutingHelper", LOG_LEVEL_INFO);

  // Enable packet captures
  wifiPhy.EnablePcapAll("vanet-sumo-dsr", true);

  // Optional: Animation trace
  AnimationInterface anim("vanet-sumo-dsr.xml");

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}