#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VANETSimulation");

int main (int argc, char *argv[])
{
  // Simulation parameters
  double simTime = 10.0;
  uint32_t packetSize = 200;
  double interval = 1.0;
  uint16_t port = 4000;

  // Create two vehicle nodes
  NodeContainer vehicleNodes;
  vehicleNodes.Create (2);

  // Configure WAVE (802.11p/DSRC) devices
  YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default ();
  YansWavePhyHelper wavePhy = YansWavePhyHelper::Default ();
  wavePhy.SetChannel (waveChannel.Create ());
  QosWaveMacHelper waveMac = QosWaveMacHelper::Default ();
  WaveHelper waveHelper = WaveHelper::Default ();
  NetDeviceContainer deviceContainer = waveHelper.Install (wavePhy, waveMac, vehicleNodes);

  // Mobility: two vehicles on a straight line, moving at constant different speeds
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));    // Vehicle 1 start
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));    // Vehicle 2 start
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (vehicleNodes);

  Ptr<ConstantVelocityMobilityModel> mobility1 = vehicleNodes.Get (0)->GetObject<ConstantVelocityMobilityModel> ();
  mobility1->SetVelocity (Vector (10.0, 0.0, 0.0)); // 10 m/s along x
  Ptr<ConstantVelocityMobilityModel> mobility2 = vehicleNodes.Get (1)->GetObject<ConstantVelocityMobilityModel> ();
  mobility2->SetVelocity (Vector (15.0, 0.0, 0.0)); // 15 m/s along x

  // Internet stack, IP assignation
  InternetStackHelper internet;
  internet.Install (vehicleNodes);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (deviceContainer);

  // UDP server (Vehicle 1), UDP client (Vehicle 2)
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (vehicleNodes.Get (0));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simTime));

  UdpClientHelper client (interfaces.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (uint32_t(simTime/interval)));
  client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp = client.Install (vehicleNodes.Get (1));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (simTime));

  // Enable NetAnim tracing
  AnimationInterface anim ("vanet-wavetwo.xml");
  anim.SetConstantPosition (vehicleNodes.Get (0), 0.0, 0.0);
  anim.SetConstantPosition (vehicleNodes.Get (1), 5.0, 0.0);

  // Assign node descriptions for NetAnim
  anim.UpdateNodeDescription (vehicleNodes.Get (0), "Vehicle1");
  anim.UpdateNodeDescription (vehicleNodes.Get (1), "Vehicle2");
  anim.UpdateNodeColor (vehicleNodes.Get (0), 0, 255, 0); // Green
  anim.UpdateNodeColor (vehicleNodes.Get (1), 0, 0, 255); // Blue

  Simulator::Stop (Seconds (simTime + 1.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}