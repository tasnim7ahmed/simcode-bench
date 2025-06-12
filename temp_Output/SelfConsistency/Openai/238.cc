/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Simulation: Mobile node communicates with a static node via 802.11s (Wi-Fi mesh).
 * The mobile node moves in a straight line and sends UDP packets to the static node.
 * The static node logs received packets.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMeshMobilityUdpExample");

// Callback function for logging received packets at the sink
void ReceivePacket (Ptr<Application> app, Ptr<Socket> socket)
{
  Address address;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom(address)))
    {
      NS_LOG_UNCOND ("Received " << packet->GetSize () << " bytes from " << InetSocketAddress::ConvertFrom(address).GetIpv4 () <<
                     " at " << Simulator::Now ().GetSeconds () << " seconds");
    }
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  uint32_t packetSize = 1024;
  double simTime = 20.0;
  double nodeSpeed = 5.0;  // meters/second
  double distance = 50.0;

  // Command line options
  CommandLine cmd;
  cmd.AddValue ("packetSize", "Size of UDP packets", packetSize);
  cmd.AddValue ("simTime", "Duration of the simulation (s)", simTime);
  cmd.AddValue ("nodeSpeed", "Speed of the mobile node (m/s)", nodeSpeed);
  cmd.AddValue ("distance", "Distance travelled by the mobile node", distance);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2); // node 0: mobile, node 1: static

  // Mobility
  MobilityHelper mobility;
  // Mobile node: node 0
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes.Get (0));
  Ptr<ConstantVelocityMobilityModel> mob =
    nodes.Get (0)->GetObject<ConstantVelocityMobilityModel> ();
  mob->SetPosition (Vector (0.0, 0.0, 0.0));
  mob->SetVelocity (Vector (nodeSpeed, 0.0, 0.0));  // Move in X direction

  // Static node: node 1
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes.Get (1));
  Ptr<ConstantPositionMobilityModel> statMob =
    nodes.Get (1)->GetObject<ConstantPositionMobilityModel> ();
  statMob->SetPosition (Vector (distance, 0.0, 0.0)); // Place at 'distance' meters from origin

  // Wi-Fi mesh configuration (802.11s)
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  MeshHelper mesh;
  mesh = MeshHelper::Default ();
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
  mesh.SetNumberOfInterfaces (1);

  NetDeviceContainer meshDevices = mesh.Install (wifiPhy, nodes);

  // Internet stack
  InternetStackHelper internetStack;
  internetStack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

  // UDP application: mobile node (0) sends to static node (1)
  uint16_t udpPort = 5000;

  // UDP Sink Application on static node
  Ptr<Socket> sinkSocket = Socket::CreateSocket (nodes.Get (1), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), udpPort);
  sinkSocket->Bind (local);
  sinkSocket->SetRecvCallback (MakeBoundCallback (&ReceivePacket, Ptr<Application> ()));

  // UDP Source Application on mobile node
  Ptr<Socket> sourceSocket = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());
  InetSocketAddress remote = InetSocketAddress (interfaces.GetAddress (1), udpPort);

  // Install a custom application to send UDP packets periodically
  class UdpCustomApp : public Application
  {
  public:
    UdpCustomApp () : m_socket (0), m_peer (), m_packetSize (0), m_nPackets (0), m_interval (Seconds (1.0)), m_sent (0) {}
    virtual ~UdpCustomApp () { m_socket = 0; }

    void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time interval)
    {
      m_socket = socket;
      m_peer = address;
      m_packetSize = packetSize;
      m_nPackets = nPackets;
      m_interval = interval;
    }

  private:
    virtual void StartApplication () override
    {
      m_socket->Connect (m_peer);
      SendPacket ();
    }
    virtual void StopApplication () override
    {
      if (m_socket)
        {
          m_socket->Close ();
        }
    }
    void SendPacket ()
    {
      Ptr<Packet> packet = Create<Packet> (m_packetSize);
      m_socket->Send (packet);
      ++m_sent;
      if (m_sent < m_nPackets)
        {
          Simulator::Schedule (m_interval, &UdpCustomApp::SendPacket, this);
        }
    }

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    Time m_interval;
    uint32_t m_sent;
  };

  uint32_t nPackets = uint32_t (simTime); // send 1 packet/sec
  Time interPacketInterval = Seconds (1.0);

  Ptr<UdpCustomApp> app = CreateObject<UdpCustomApp> ();
  app->Setup (sourceSocket, remote, packetSize, nPackets, interPacketInterval);
  nodes.Get (0)->AddApplication (app);
  app->SetStartTime (Seconds (1.0));
  app->SetStopTime (Seconds (simTime));

  Simulator::Stop (Seconds (simTime + 1.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}