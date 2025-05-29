#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-net-device.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("V2VWaveSimulation");

static void
ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  if (packet->GetSize() > 0)
    {
      NS_LOG_INFO ("Received one packet!");
    }
}

int
main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nVehicles = 2;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("numVehicles", "Number of vehicles in simulation", nVehicles);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("V2VWaveSimulation", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer vehicles;
  vehicles.Create (nVehicles);

  Wifi80211pHelper wifi80211pHelper;
  WaveMacHelper waveMacHelper;
  WaveHelper waveHelper;

  waveHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue ("OfdmRate6MbpsBW10MHz"),
                                      "ControlMode", StringValue ("OfdmRate6MbpsBW10MHz"));

  NetDeviceContainer devices = waveHelper.Install (waveMacHelper, vehicles);

  InternetStackHelper internet;
  internet.Install (vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (nVehicles),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (vehicles);

  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer apps = server.Install (vehicles.Get (1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (simulationTime - 1));

  UdpClientHelper client (i.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (100));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client.Install (vehicles.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (simulationTime - 2));

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}