#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {

  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  Time packetInterval = Seconds(1.0);
  uint8_t tosValue = 0xa0;
  uint8_t ttlValue = 64;
  bool recvTos = false;
  bool recvTtl = false;

  CommandLine cmd;
  cmd.AddValue("packetSize", "Size of the packets sent", packetSize);
  cmd.AddValue("numPackets", "Number of packets sent", numPackets);
  cmd.AddValue("packetInterval", "Interval between packets", packetInterval);
  cmd.AddValue("tos", "IP TOS value", tosValue);
  cmd.AddValue("ttl", "IP TTL value", ttlValue);
  cmd.AddValue("recvTos", "Enable SO_RECVTOS", recvTos);
  cmd.AddValue("recvTtl", "Enable SO_RECVTTL", recvTtl);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
  echoClient.SetAttribute("Interval", TimeValue(packetInterval));
  echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Ptr<Application> app = serverApps.Get(0);
  Ptr<UdpEchoServer> server = DynamicCast<UdpEchoServer>(app);
  server->SetRecvTOS(recvTos);
  server->SetRecvTTL(recvTtl);

  Ptr<Socket> sock = clientApps.Get(0)->GetObject<UdpEchoClient>()->GetSocket();
  sock->SetIpTtl(ttlValue);
  sock->SetTos(tosValue);

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}