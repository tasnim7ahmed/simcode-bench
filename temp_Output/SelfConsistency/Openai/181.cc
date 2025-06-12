/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-helper.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VANETDsrcWaveExample");

class UdpPacketSink : public Application
{
public:
  UdpPacketSink () {}
  virtual ~UdpPacketSink () {}

  void Setup(Address address, uint16_t port)
  {
    m_address = address;
    m_port = port;
  }

protected:
  virtual void StartApplication () override
  {
    if (!m_socket)
      {
        m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
        InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
        m_socket->Bind (local);
        m_socket->SetRecvCallback (MakeCallback (&UdpPacketSink::HandleRead, this));
      }
  }
  virtual void StopApplication () override
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket = nullptr;
      }
  }

private:
  void HandleRead (Ptr<Socket> socket)
  {
    while (Ptr<Packet> packet = socket->Recv ())
      {
        NS_LOG_INFO (Simulator::Now ().GetSeconds ()
                     << "s: Received "
                     << packet->GetSize ()
                     << " bytes at node "
                     << GetNode ()->GetId ());
      }
  }

  Ptr<Socket> m_socket;
  Address m_address;
  uint16_t m_port;
};

int main (int argc, char *argv[])
{
  LogComponentEnable ("VANETDsrcWaveExample", LOG_LEVEL_INFO);

  // Simulation parameters
  uint32_t numVehicles = 2;
  double simTime = 10.0;
  double distance = 100.0; // meters between the vehicles at t=0
  double speed = 15.0; // m/s

  // 1. Create nodes (vehicles)
  NodeContainer vehicles;
  vehicles.Create (numVehicles);

  // 2. Mobility: Straight road, vehicles move along X, same speed, opposite directions
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
  posAlloc->Add (Vector (0.0, 0.0, 0.0));
  posAlloc->Add (Vector (distance, 0.0, 0.0));
  mobility.SetPositionAllocator (posAlloc);
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (vehicles);

  // Set velocities: vehicle 0 moves +X, vehicle 1 moves -X
  Ptr<ConstantVelocityMobilityModel> mob0 = vehicles.Get (0)->GetObject<ConstantVelocityMobilityModel> ();
  Ptr<ConstantVelocityMobilityModel> mob1 = vehicles.Get (1)->GetObject<ConstantVelocityMobilityModel> ();
  mob0->SetVelocity (Vector (speed, 0.0, 0.0));
  mob1->SetVelocity (Vector (-speed, 0.0, 0.0));

  // 3. DSRC/WAVE PHY/MAC
  YansWavePhyHelper wavePhy = YansWavePhyHelper::Default ();
  YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default ();
  wavePhy.SetChannel (waveChannel.Create ());

  NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default ();
  WaveHelper waveHelper = WaveHelper::Default ();
  NetDeviceContainer devices = waveHelper.Install (wavePhy, waveMac, vehicles);

  // 4. Internet stack & IP addresses
  InternetStackHelper internet;
  internet.Install (vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // 5. UDP Applications: node 0 sends, node 1 receives
  uint16_t port = 5000;

  // UDP PacketSink (receiver) on node 1
  Ptr<UdpPacketSink> sinkApp = CreateObject<UdpPacketSink> ();
  sinkApp->Setup (interfaces.GetAddress (1), port);
  vehicles.Get (1)->AddApplication (sinkApp);
  sinkApp->SetStartTime (Seconds (0.0));
  sinkApp->SetStopTime (Seconds (simTime));

  // UDP OnOffApplication (sender) on node 0
  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     InetSocketAddress (interfaces.GetAddress (1), port));
  onoff.SetConstantRate (DataRate ("512kbps"), 200); // 200 bytes per packet
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));

  ApplicationContainer app = onoff.Install (vehicles.Get (0));

  // 6. NetAnim Setup
  AnimationInterface anim ("vanet-dsrc-wave.xml");
  // Optional: set node descriptions and colors
  anim.SetConstantPosition (vehicles.Get (0), 0.0, 0.0);
  anim.SetConstantPosition (vehicles.Get (1), distance, 0.0);
  anim.UpdateNodeDescription (vehicles.Get (0), "Vehicle 0");
  anim.UpdateNodeDescription (vehicles.Get (1), "Vehicle 1");
  anim.UpdateNodeColor (vehicles.Get (0), 255, 0, 0);
  anim.UpdateNodeColor (vehicles.Get (1), 0, 0, 255);

  // Show mobility in NetAnim by updating positions dynamically
  anim.EnablePacketMetadata (true);

  // 7. Run simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}