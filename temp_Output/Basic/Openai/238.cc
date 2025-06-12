#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMeshAdhocMovementExample");

void ReceivePacket (Ptr<Socket> socket)
{
  while (Ptr<Packet> packet = socket->Recv ())
    {
      NS_LOG_UNCOND ("Received packet of size " << packet->GetSize() << " bytes at " << Simulator::Now ().GetSeconds () << "s");
    }
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  MeshHelper mesh = MeshHelper::Default ();
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
  mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));

  NetDeviceContainer meshDevices = mesh.Install (phy, nodes);

  InternetStackHelper internetStack;
  internetStack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

  MobilityHelper mobility;

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes.Get (1)); // static node

  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));  // Mobile node initial
  positionAlloc->Add (Vector (50.0, 0.0, 0.0)); // Static node
  mobility.SetPositionAllocator (positionAlloc);

  // Set up mobile node movement
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes.Get (0));
  Ptr<ConstantVelocityMobilityModel> mob = nodes.Get (0)->GetObject<ConstantVelocityMobilityModel> ();
  mob->SetPosition (Vector (0.0, 0.0, 0.0));
  mob->SetVelocity (Vector (5.0, 0.0, 0.0)); // move at 5 m/s along X

  uint16_t port = 5000;

  // UDP server socket on static node
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSocket = Socket::CreateSocket (nodes.Get (1), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  recvSocket->Bind (local);

  recvSocket->SetRecvCallback (MakeCallback (&ReceivePacket));

  // UDP client application on mobile node
  Ptr<Socket> source = Socket::CreateSocket (nodes.Get (0), tid);
  InetSocketAddress remote (interfaces.GetAddress (1), port);
  source->Connect (remote);

  // Generate UDP traffic: send packet every 0.1s
  uint32_t pktSize = 512;
  uint32_t numPkts = 100;
  Time interPacketInterval = Seconds (0.1);

  for (uint32_t i = 0; i < numPkts; ++i)
    {
      Simulator::Schedule (Seconds (i * 0.1), &Socket::Send, source, Create<Packet>(pktSize));
    }

  Simulator::Stop (Seconds (numPkts * 0.1 + 1));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}