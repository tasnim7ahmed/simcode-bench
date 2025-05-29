#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lorawan-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/log.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LoraWanSensor");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetEnableAllLevels (LOG_COMPONENT_NAME);

  NodeContainer nodes;
  nodes.Create (2);

  // LoRaWAN Helper
  LoraWanHelper lorawanHelper;
  lorawanHelper.EnablePacketTracking ();

  // Configure the LoRaWAN MAC parameters
  lorawanHelper.SetSpreadingFactorsUp (nodes, {12});

  // Create the LoRaWAN channels
  Ptr<LoraChannel> channel = CreateObject<LoraChannel> ();
  lorawanHelper.AssignChannel (channel);

  // Install the LoRaWAN device on the nodes
  NetDeviceContainer devices = lorawanHelper.Install (nodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("50.0"),
                                 "Y", StringValue ("50.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=30]"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  // UDP Server (Sink)
  UdpServerHelper server (5000);
  ApplicationContainer apps = server.Install (nodes.Get (1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  // UDP Client (Sensor)
  UdpClientHelper client (i.GetAddress (1), 5000);
  client.SetAttribute ("MaxPackets", UintegerValue (10));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (10));
  apps = client.Install (nodes.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));

  // Logging received packets at sink node
  Packet::EnablePrinting ();
  Simulator::Stop (Seconds (10.0));

  // Create a trace file
  std::ofstream ascii;
  ascii.open ("lorawan-sensor.tr");

  Simulator::Schedule (Seconds(0.1), [&ascii](){
          ascii << "Starting Simulation" << std::endl;
        });
  Simulator::ScheduleDestroy ();
  Simulator::Run ();
  ascii.close();

  return 0;
}