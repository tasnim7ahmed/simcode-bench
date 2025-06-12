#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpLan");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  Time interPacketInterval = Seconds (1.0);
  uint8_t tosValue = 0;
  uint8_t ttlValue = 64;
  bool recvTos = false;
  bool recvTtl = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application containers to log if set to true.", verbose);
  cmd.AddValue ("packetSize", "Size of packets sent", packetSize);
  cmd.AddValue ("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue ("interval", "Interval between packets", interPacketInterval);
  cmd.AddValue ("tos", "IP TOS value", tosValue);
  cmd.AddValue ("ttl", "IP TTL value", ttlValue);
  cmd.AddValue ("recvTos", "Receive TOS value", recvTos);
  cmd.AddValue ("recvTtl", "Receive TTL value", recvTtl);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  echoClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ptr<Socket> sock = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());
  sock->SetIpTtl (ttlValue);
  sock->SetTos (tosValue);

  if (recvTos)
    {
      Ptr<UdpEchoServer> server = DynamicCast<UdpEchoServer>(serverApps.Get(0));
      server->SetReceiveTos(true);
    }

  if (recvTtl)
    {
      Ptr<UdpEchoServer> server = DynamicCast<UdpEchoServer>(serverApps.Get(0));
      server->SetReceiveTtl(true);
    }

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}