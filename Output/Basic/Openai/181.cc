#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetDsrcWaveSimulation");

void ReceivePacket (Ptr<Socket> socket)
{
  while (Ptr<Packet> pkt = socket->Recv ())
    {
    }
}

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer vehicles;
  vehicles.Create (2);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
  NetDeviceContainer devices = wifi80211p.Install (wifiPhy, wifi80211pMac, vehicles);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (50.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");

  mobility.Install (vehicles);
  Ptr<ConstantVelocityMobilityModel> mob0 = vehicles.Get (0)->GetObject<ConstantVelocityMobilityModel> ();
  Ptr<ConstantVelocityMobilityModel> mob1 = vehicles.Get (1)->GetObject<ConstantVelocityMobilityModel> ();
  mob0->SetVelocity (Vector (10.0, 0.0, 0.0));
  mob1->SetVelocity (Vector (15.0, 0.0, 0.0));

  InternetStackHelper internet;
  internet.Install (vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  uint16_t port = 4000;

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (vehicles.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (320));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (50)));
  client.SetAttribute ("PacketSize", UintegerValue (200));
  ApplicationContainer clientApps = client.Install (vehicles.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  AnimationInterface anim ("vanet-dsrc-wave.xml");
  anim.SetConstantPosition (vehicles.Get (0), 0, 0);
  anim.SetConstantPosition (vehicles.Get (1), 50, 0);

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}