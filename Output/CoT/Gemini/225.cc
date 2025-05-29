#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-net-device.h"
#include "ns3/v4-ping.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VANET-DSDV");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nVehicles = 10;
  double simulationTime = 30;
  double distance = 50;
  std::string phyMode ("OfdmRate6MbpsBW10MHz");

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("nVehicles", "Number of vehicles", nVehicles);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance between nodes", distance);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  LogComponent::SetCategory ("UdpEchoClientApplication");
  LogComponent::SetCategory ("UdpEchoServerApplication");

  NodeContainer rsu;
  rsu.Create (1);

  NodeContainer vehicles;
  vehicles.Create (nVehicles);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (nVehicles),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::WaypointMobilityModel");
  mobility.Install (vehicles);

  Ptr<WaypointMobilityModel> mobilityModelRsu = CreateObject<WaypointMobilityModel> ();
  mobilityModelRsu->SetPosition (Vector (distance * (nVehicles / 2), distance * 2, 0));
  rsu.Get (0)->AggregateObject (mobilityModelRsu);

  for (uint32_t i = 0; i < nVehicles; ++i)
  {
    Ptr<WaypointMobilityModel> mobilityModel = vehicles.Get (i)->GetObject<WaypointMobilityModel> ();
    Waypoint wp0 (Seconds (0.0), Vector (i * distance, 0, 0));
    Waypoint wp1 (Seconds (simulationTime), Vector (i * distance + 100, 0, 0));
    mobilityModel->AddWaypoint (wp0);
    mobilityModel->AddWaypoint (wp1);
  }

  WifiHelper wifi;
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  QosWaveMacHelper wifiMac = QosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211p);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue (phyMode),
                                "ControlMode", StringValue (phyMode));

  NetDeviceContainer devices = wifi80211p.Install (wifiPhy, wifiMac, vehicles);
  NetDeviceContainer rsuDevices = wifi80211p.Install (wifiPhy, wifiMac, rsu);

  InternetStackHelper internet;
  internet.Install (rsu);
  internet.Install (vehicles);

  DsdvHelper dsdv;
  internet.SetRoutingHelper (dsdv);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (rsu, devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (rsu.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpEchoClientHelper echoClient (i.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  for(uint32_t j = 0; j < nVehicles; ++j) {
    clientApps.Add (echoClient.Install (vehicles.Get (j)));
  }

  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}