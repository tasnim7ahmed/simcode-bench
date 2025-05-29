#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/propagation-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/buildings-module.h"
#include "ns3/netanim-module.h"
#include "ns3/log.h"

#include <iostream>
#include <fstream>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VehicularSimulation");

double CalculatePathloss3GPP38901 (double distance, double frequency)
{
  double pathloss = 0.0;
  double fc = frequency / 1e9; 
  
  if (distance < 10)
  {
      distance = 10;
  }

  double los = 1.0; 
  pathloss = 28.0 + 22.0 * std::log10 (distance) + 20.0 * std::log10 (fc); 

  return pathloss;
}

int main (int argc, char *argv[])
{
  bool enableNetAnim = false;
  std::string phyMode ("OfdmRate6MbpsBW10MHz");
  double simulationTime = 10.0;
  double frequency = 28e9;

  CommandLine cmd;
  cmd.AddValue ("EnableNetAnim", "Enable network animator GUI", enableNetAnim);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("frequency", "Carrier frequency in Hz", frequency);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiMac::Ssid", SsidValue (Ssid ("ns-3-ssid")));
  
  NodeContainer vehicles;
  vehicles.Create (2);

  NodeContainer baseStation;
  baseStation.Create (1);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("100.0"),
                                 "Y", StringValue ("100.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("1s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10]"),
                             "Bounds", StringValue ("150x150"));
  mobility.Install (vehicles);

  MobilityHelper baseStationMobility;
  baseStationMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  baseStationMobility.Install (baseStation);
  baseStation.Get (0)->GetObject<MobilityModel> ()->SetPosition (Vector (100.0, 100.0, 0.0));
  
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("RxGain", DoubleValue (10));
  wifiPhy.Set ("TxGain", DoubleValue (10));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue (3.5));

  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer vehicleDevices = wifi.Install (wifiPhy, wifiMac, vehicles);
  NetDeviceContainer baseStationDevices = wifi.Install (wifiPhy, wifiMac, baseStation);

  InternetStackHelper stack;
  stack.Install (vehicles);
  stack.Install (baseStation);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer vehicleInterfaces = address.Assign (vehicleDevices);
  Ipv4InterfaceContainer baseStationInterfaces = address.Assign (baseStationDevices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (baseStation.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1));

  UdpEchoClientHelper echoClient (baseStationInterfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (vehicles.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 2));
  
  std::ofstream snrStream("snr.dat");
  std::ofstream pathlossStream("pathloss.dat");

  Simulator::Schedule (Seconds (0.1), [&]() {
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
      Ptr<MobilityModel> mobilityVehicle = vehicles.Get (i)->GetObject<MobilityModel> ();
      Ptr<MobilityModel> mobilityBaseStation = baseStation.Get (0)->GetObject<MobilityModel> ();

      Vector vehiclePosition = mobilityVehicle->GetPosition ();
      Vector baseStationPosition = mobilityBaseStation->GetPosition ();

      double distance = std::sqrt (std::pow (vehiclePosition.x - baseStationPosition.x, 2) +
                                    std::pow (vehiclePosition.y - baseStationPosition.y, 2));

      double pathloss = CalculatePathloss3GPP38901 (distance, frequency);
      double txPower = 20.0; 
      double noiseFloor = -100.0; 

      double receivedPower = txPower - pathloss;
      double snr = receivedPower - noiseFloor;

      snrStream << Simulator::Now ().GetSeconds () << " " << snr << std::endl;
      pathlossStream << Simulator::Now ().GetSeconds () << " " << pathloss << std::endl;
    }
  });

  Simulator::Schedule (Seconds (1.0), [&]() {
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
      Ptr<MobilityModel> mobilityVehicle = vehicles.Get (i)->GetObject<MobilityModel> ();
      Ptr<MobilityModel> mobilityBaseStation = baseStation.Get (0)->GetObject<MobilityModel> ();

      Vector vehiclePosition = mobilityVehicle->GetPosition ();
      Vector baseStationPosition = mobilityBaseStation->GetPosition ();

      double distance = std::sqrt (std::pow (vehiclePosition.x - baseStationPosition.x, 2) +
                                    std::pow (vehiclePosition.y - baseStationPosition.y, 2));

      double pathloss = CalculatePathloss3GPP38901 (distance, frequency);
      double txPower = 20.0;
      double noiseFloor = -100.0;

      double receivedPower = txPower - pathloss;
      double snr = receivedPower - noiseFloor;

      snrStream << Simulator::Now ().GetSeconds () << " " << snr << std::endl;
      pathlossStream << Simulator::Now ().GetSeconds () << " " << pathloss << std::endl;
    }
  });

  Simulator::Schedule (Seconds (2.0), [&]() {
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
      Ptr<MobilityModel> mobilityVehicle = vehicles.Get (i)->GetObject<MobilityModel> ();
      Ptr<MobilityModel> mobilityBaseStation = baseStation.Get (0)->GetObject<MobilityModel> ();

      Vector vehiclePosition = mobilityVehicle->GetPosition ();
      Vector baseStationPosition = mobilityBaseStation->GetPosition ();

      double distance = std::sqrt (std::pow (vehiclePosition.x - baseStationPosition.x, 2) +
                                    std::pow (vehiclePosition.y - baseStationPosition.y, 2));

      double pathloss = CalculatePathloss3GPP38901 (distance, frequency);
      double txPower = 20.0;
      double noiseFloor = -100.0;

      double receivedPower = txPower - pathloss;
      double snr = receivedPower - noiseFloor;

      snrStream << Simulator::Now ().GetSeconds () << " " << snr << std::endl;
      pathlossStream << Simulator::Now ().GetSeconds () << " " << pathloss << std::endl;
    }
  });
  
  Simulator::Schedule (Seconds (3.0), [&]() {
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
      Ptr<MobilityModel> mobilityVehicle = vehicles.Get (i)->GetObject<MobilityModel> ();
      Ptr<MobilityModel> mobilityBaseStation = baseStation.Get (0)->GetObject<MobilityModel> ();

      Vector vehiclePosition = mobilityVehicle->GetPosition ();
      Vector baseStationPosition = mobilityBaseStation->GetPosition ();

      double distance = std::sqrt (std::pow (vehiclePosition.x - baseStationPosition.x, 2) +
                                    std::pow (vehiclePosition.y - baseStationPosition.y, 2));

      double pathloss = CalculatePathloss3GPP38901 (distance, frequency);
      double txPower = 20.0;
      double noiseFloor = -100.0;

      double receivedPower = txPower - pathloss;
      double snr = receivedPower - noiseFloor;

      snrStream << Simulator::Now ().GetSeconds () << " " << snr << std::endl;
      pathlossStream << Simulator::Now ().GetSeconds () << " " << pathloss << std::endl;
    }
  });
  
  Simulator::Schedule (Seconds (4.0), [&]() {
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
      Ptr<MobilityModel> mobilityVehicle = vehicles.Get (i)->GetObject<MobilityModel> ();
      Ptr<MobilityModel> mobilityBaseStation = baseStation.Get (0)->GetObject<MobilityModel> ();

      Vector vehiclePosition = mobilityVehicle->GetPosition ();
      Vector baseStationPosition = mobilityBaseStation->GetPosition ();

      double distance = std::sqrt (std::pow (vehiclePosition.x - baseStationPosition.x, 2) +
                                    std::pow (vehiclePosition.y - baseStationPosition.y, 2));

      double pathloss = CalculatePathloss3GPP38901 (distance, frequency);
      double txPower = 20.0;
      double noiseFloor = -100.0;

      double receivedPower = txPower - pathloss;
      double snr = receivedPower - noiseFloor;

      snrStream << Simulator::Now ().GetSeconds () << " " << snr << std::endl;
      pathlossStream << Simulator::Now ().GetSeconds () << " " << pathloss << std::endl;
    }
  });
  
  Simulator::Schedule (Seconds (5.0), [&]() {
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
      Ptr<MobilityModel> mobilityVehicle = vehicles.Get (i)->GetObject<MobilityModel> ();
      Ptr<MobilityModel> mobilityBaseStation = baseStation.Get (0)->GetObject<MobilityModel> ();

      Vector vehiclePosition = mobilityVehicle->GetPosition ();
      Vector baseStationPosition = mobilityBaseStation->GetPosition ();

      double distance = std::sqrt (std::pow (vehiclePosition.x - baseStationPosition.x, 2) +
                                    std::pow (vehiclePosition.y - baseStationPosition.y, 2));

      double pathloss = CalculatePathloss3GPP38901 (distance, frequency);
      double txPower = 20.0;
      double noiseFloor = -100.0;

      double receivedPower = txPower - pathloss;
      double snr = receivedPower - noiseFloor;

      snrStream << Simulator::Now ().GetSeconds () << " " << snr << std::endl;
      pathlossStream << Simulator::Now ().GetSeconds () << " " << pathloss << std::endl;
    }
  });

  Simulator::Stop (Seconds (simulationTime));

  if (enableNetAnim)
  {
    AnimationInterface anim ("vehicular-simulation.xml");
  }

  Simulator::Run ();

  snrStream.close();
  pathlossStream.close();

  Simulator::Destroy ();
  return 0;
}