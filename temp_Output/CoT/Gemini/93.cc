#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet.h"
#include <iostream>
#include <fstream>

using namespace ns3;

uint16_t g_port = 9;
uint32_t g_received = 0;

struct PacketInfo {
    uint8_t tos;
    uint8_t ttl;
};

static void
ReceivePacket (Ptr<Socket> socket)
{
  Address localAddress;
  Ptr<Packet> packet = socket->RecvFrom (localAddress);
  g_received++;

  InetSocketAddress iaddr = InetSocketAddress::ConvertFrom (localAddress);
  Ipv4Address senderAddress = iaddr.GetIpv4 ();
  uint16_t senderPort = iaddr.GetPort ();

  PacketInfo info;
  packet->CopyData((uint8_t*)&info, sizeof(PacketInfo));

  NS_LOG_UNCOND ("Received one packet from " << senderAddress << " port " << senderPort
                   << " at time " << Simulator::Now ().GetSeconds ()
                   << "s packet size " << packet->GetSize() << " bytes"
                   << " Tos " << (uint16_t)info.tos
                   << " Ttl " << (uint16_t)info.ttl);
}


int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nPackets = 1;
  Time interPacketInterval = Seconds (0.1);
  uint32_t packetSize = 1024;
  uint8_t tosValue = 0;
  uint8_t ttlValue = 64;
  bool recvTos = false;
  bool recvTtl = false;


  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("nPackets", "Number of packets sent", nPackets);
  cmd.AddValue ("packetSize", "Size of packets sent", packetSize);
  cmd.AddValue ("interval", "Inter packet interval", interPacketInterval);
  cmd.AddValue ("tos", "IP TOS value", tosValue);
  cmd.AddValue ("ttl", "IP TTL value", ttlValue);
  cmd.AddValue ("recvTos", "Enable TOS retrieval", recvTos);
  cmd.AddValue ("recvTtl", "Enable TTL retrieval", recvTtl);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (g_port);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  Ptr<Socket> recvSocket = Socket::CreateSocket (nodes.Get (1), TypeId::LookupByName ("ns3::UdpSocketFactory"));
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), g_port);
  recvSocket->Bind (local);
  recvSocket->SetRecvCallback (MakeCallback (&ReceivePacket));

  if (recvTos)
    {
      recvSocket->SetAttribute("IpRecvTos", BooleanValue(true));
    }
  if (recvTtl)
    {
      recvSocket->SetAttribute("IpRecvTtl", BooleanValue(true));
    }


  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), g_port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (nPackets));
  echoClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ptr<Socket> sendSocket = Socket::CreateSocket (nodes.Get (0), TypeId::LookupByName ("ns3::UdpSocketFactory"));
  sendSocket->SetAttribute ("IpTos", UintegerValue (tosValue));
  sendSocket->SetAttribute ("IpTtl", UintegerValue (ttlValue));

  Simulator::Schedule (Seconds(2.1), [&,packetSize]() {
      for (uint32_t i = 0; i < nPackets; ++i) {
          Ptr<Packet> packet = Create<Packet> (packetSize);
          PacketInfo info;
          info.tos = tosValue;
          info.ttl = ttlValue;
          packet->AddAtStart ((uint8_t*)&info, sizeof(PacketInfo));
          sendSocket->SendTo (packet, 0, InetSocketAddress (interfaces.GetAddress (1), g_port));
          Simulator::Schedule (interPacketInterval * (i+1), [](){});
      }
  });


  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  NS_LOG_UNCOND("Total packets received: " << g_received);
  return 0;
}