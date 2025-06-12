/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Minimal LoRaWAN simulation: one sensor periodically sends UDP data
 * to one sink node. The sink logs received data.
 *
 * Requires NS-3 modules: lorawan, internet, applications.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/lorawan-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MinimalLoraUdpSimulation");

// Custom application to log all received UDP packets at the sink
class UdpSinkLogger : public Application
{
public:
  UdpSinkLogger () {}
  virtual ~UdpSinkLogger () {}

protected:
  void StartApplication () override
  {
    if (!m_socket)
      {
        m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
        InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
        m_socket->Bind (local);
        m_socket->SetRecvCallback (MakeCallback (&UdpSinkLogger::HandleRead, this));
      }
  }
  void StopApplication () override
  {
    if (m_socket)
      {
        m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
        m_socket->Close ();
        m_socket = 0;
      }
  }
  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        NS_LOG_UNCOND ("[" << Simulator::Now ().GetSeconds ()
                         << "s] Sink received " << packet->GetSize ()
                         << " bytes from "
                         << InetSocketAddress::ConvertFrom (from).GetIpv4 ());
      }
  }
public:
  void SetPort (uint16_t port)
  {
    m_port = port;
  }
private:
  Ptr<Socket> m_socket;
  uint16_t m_port;
};

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Parameters
  uint16_t udpPort = 4000;
  double simDuration = 30.0;
  double sendInterval = 5.0; // seconds

  // Create nodes: end devices and gateway (+network server)
  NodeContainer endDevices;
  endDevices.Create (2); // 0: sensor, 1: sink

  NodeContainer gateways;
  gateways.Create (1);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // sensor
  positionAlloc->Add (Vector (5.0, 0.0, 0.0)); // sink
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (endDevices);

  Ptr<ListPositionAllocator> gwPos = CreateObject<ListPositionAllocator> ();
  gwPos->Add (Vector (2.5, 0.0, 15.0)); // elevated gateway
  mobility.SetPositionAllocator (gwPos);
  mobility.Install (gateways);

  // Lora channel, helpers & devices
  Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel> ();
  loss->SetPathLossExponent (3.76);
  Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel> ();
  Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel> ();
  channel->AddPropagationLossModel (loss);
  channel->SetPropagationDelayModel (delay);

  LorawanHelper lorawan;
  lorawan.EnablePacketTracking (); // (optional) for easier future debugging

  // Set up EU868 Bands
  lorawan.SetSpreadingFactorsUp (endDevices, gateways, channel);

  // LoraNetDevices
  NetDeviceContainer edDevs = lorawan.Install (endDevices, gateways, channel);

  // Internet stack only on end devices
  InternetStackHelper internet;
  internet.Install (endDevices);

  // Assign IP addresses from a /29 subnet
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.248");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (edDevs);

  // Traffic: UDP from sensor (node 0) to sink (node 1)
  // Install UDP Server (custom logger) on sink
  Ptr<UdpSinkLogger> sinkApp = CreateObject<UdpSinkLogger> ();
  sinkApp->SetPort (udpPort);
  endDevices.Get (1)->AddApplication (sinkApp);
  sinkApp->SetStartTime (Seconds (0.0));
  sinkApp->SetStopTime (Seconds (simDuration + 1));

  // Install UDP client on sensor: periodic, small packets
  UdpClientHelper clientHelper (interfaces.GetAddress (1), udpPort); // sink ip/port
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (uint32_t (simDuration / sendInterval + 2)));
  clientHelper.SetAttribute ("Interval", TimeValue (Seconds (sendInterval)));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (16));
  ApplicationContainer clientApps = clientHelper.Install (endDevices.Get (0)); // only sensor
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simDuration));

  // Disable ARQ and fragmentation for simplification
  lorawan.SetDeviceAttribute (edDevs, "EnableFragmentation", BooleanValue (false));
  lorawan.SetDeviceAttribute (edDevs, "EnableRetransmissions", BooleanValue (false));

  // Start simulation
  Simulator::Stop (Seconds (simDuration + 2));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}