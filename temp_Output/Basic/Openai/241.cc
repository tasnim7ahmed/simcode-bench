#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetWaveVehicleRsu");

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2); // node 0: vehicle, node 1: RSU

  // Mobility: RSU static, vehicle moves in a straight line
  MobilityHelper mobilityVehicle;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));    // Vehicle start
  positionAlloc->Add (Vector (100.0, 0.0, 0.0));  // RSU
  mobilityVehicle.SetPositionAllocator (positionAlloc);
  mobilityVehicle.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobilityVehicle.Install (nodes.Get (0));
  Ptr<ConstantVelocityMobilityModel> vehicleMob = nodes.Get (0)->GetObject<ConstantVelocityMobilityModel> ();
  vehicleMob->SetVelocity (Vector (20.0, 0.0, 0.0)); // 20 m/s along x

  MobilityHelper mobilityRsu;
  mobilityRsu.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityRsu.Install (nodes.Get (1));

  // WAVE 802.11p setup
  YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default ();
  YansWavePhyHelper wavePhy = YansWavePhyHelper::Default ();
  wavePhy.SetChannel (waveChannel.Create ());
  QosWaveMacHelper waveMac = QosWaveMacHelper::Default ();
  Wifi80211pHelper waveHelper = Wifi80211pHelper::Default ();

  NetDeviceContainer devices = waveHelper.Install (wavePhy, waveMac, nodes);

  // IP stack
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // UDP server on RSU (node 1)
  uint16_t port = 4000;
  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (1));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // UDP client on vehicle (node 0), periodic traffic
  uint32_t packetSize = 200;
  uint32_t maxPackets = 50;
  double interval = 0.2; // seconds
  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}