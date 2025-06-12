#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/on-off-helper.h"
#include "ns3/yans-wifi-phy.h"
#include "ns3/v4-ping.h"
#include "ns3/v4ping-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VANETSimulation");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;
  uint32_t nVehicles = 5;
  double simulationTime = 10;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("numVehicles", "Number of vehicles", nVehicles);
  cmd.AddValue ("simulationTime", "Simulation time (s)", simulationTime);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("VANETSimulation", LOG_LEVEL_INFO);
    }

  NodeContainer vehicles;
  vehicles.Create (nVehicles);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("0"),
                                 "Y", StringValue ("0"),
                                 "Z", StringValue ("0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=50]"));
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (vehicles);

  for (uint32_t i = 0; i < nVehicles; ++i)
    {
      Ptr<ConstantVelocityMobilityModel> vehicleMobility = vehicles.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      Vector3D velocity (10, 0, 0);
      vehicleMobility->SetVelocity (velocity);
    }

  Wifi80211pHelper wifi80211pHelper;
  YansWifiPhyHelper phy = wifi80211pHelper.GetPhy ();
  YansWifiChannelHelper channel = wifi80211pHelper.GetChannel ();
  phy.SetChannel (channel.Create ());

  WaveMacHelper macHelper;
  WifiHelper wifiHelper;
  wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6MbpsBW10MHz"), "ControlMode", StringValue ("OfdmRate6MbpsBW10MHz"));
  NetDeviceContainer devices = wifiHelper.Install (phy, macHelper.CreateOcbWifiMac (OcbWifiMacHelper::OcbMode::OCB_SCH), vehicles);

  InternetStackHelper internet;
  internet.Install (vehicles);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (1), port)));
  onoff.SetConstantRate (DataRate ("1Mbps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (1000));

  ApplicationContainer app = onoff.Install (vehicles.Get (0));
  app.Start (Seconds (1.0));
  app.Stop (Seconds (simulationTime));

  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (vehicles.Get (1));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (simulationTime));


  if (tracing)
    {
      phy.EnablePcapAll ("vanet");
    }

  AnimationInterface anim ("vanet.xml");
  anim.SetMaxPktsPerTraceFile(5000000);

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}