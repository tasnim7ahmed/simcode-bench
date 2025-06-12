/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Simulation of a VANET: Vehicles communicate via UDP over Wi-Fi with DSR routing.
 * Vehicles follow a realistic mobility model loaded from a SUMO-generated mobility trace.
 * Simulation runs for 80 seconds.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VANET-SUMO-DSR-UDP");

// Function to install a UDP echo application on src, targeting dst
void InstallUdpEchoApps(Ptr<Node> src, Ipv4Address dstAddr, uint16_t port, double start, double stop)
{
  UdpEchoClientHelper echoClientHelper(dstAddr, port);
  echoClientHelper.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(1)));
  echoClientHelper.SetAttribute("PacketSize", UintegerValue(64));
  ApplicationContainer clientApp = echoClientHelper.Install(src);
  clientApp.Start(Seconds(start));
  clientApp.Stop(Seconds(stop));
}

// Entry point
int main (int argc, char *argv[])
{
  uint32_t numVehicles = 10;
  double simTime = 80.0;
  std::string sumoTraceFile = "vanet-sumo-mobility.tcl"; // Generated from SUMO tools

  CommandLine cmd;
  cmd.AddValue ("numVehicles", "Number of vehicle nodes in the VANET", numVehicles);
  cmd.AddValue ("sumoTraceFile", "SUMO mobility trace file (TCL/NS-2 format)", sumoTraceFile);
  cmd.Parse (argc, argv);

  // Create vehicle nodes
  NodeContainer vehicles;
  vehicles.Create (numVehicles);

  // Wi-Fi PHY and MAC configuration
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (16.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (16.0));
  wifiPhy.Set ("RxGain", DoubleValue(0));
  wifiPhy.Set ("TxGain", DoubleValue(0));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue(-90.0));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue(-90.0));

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");

  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer wifiDevices = wifi.Install (wifiPhy, wifiMac, vehicles);

  // Install SUMO-generated mobility trace (NS2MobilityHelper expects TCL file)
  Ns2MobilityHelper ns2Mobility (sumoTraceFile);
  ns2Mobility.Install();

  // Install Internet stack, use DSR for routing
  InternetStackHelper stack;
  DsrMainHelper dsrMainHelper;
  DsrHelper dsrHelper;
  stack.SetRoutingHelper (dsrHelper);
  stack.Install (vehicles);

  // Assign IPv4 addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer vehicleInterfaces = address.Assign (wifiDevices);

  // Install UDP Echo Server on the last node
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (vehicles.Get (numVehicles - 1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simTime));

  // Install UDP Echo Client on the first node (to communicate with last node)
  InstallUdpEchoApps (vehicles.Get (0),
                      vehicleInterfaces.GetAddress (numVehicles - 1),
                      echoPort,
                      2.0,
                      simTime);

  // Add additional flows (optional): Intermediate vehicle nodes can also transmit
  // for (uint32_t i = 1; i < numVehicles - 1; ++i)
  // {
  //   InstallUdpEchoApps (vehicles.Get (i),
  //                       vehicleInterfaces.GetAddress ((i+1)%numVehicles),
  //                       echoPort,
  //                       3.0 + i,
  //                       simTime);
  // }

  // Enable packet capture for all devices
  wifiPhy.EnablePcapAll ("vanet-sumo-dsr");

  // Set logging, if needed
  // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}