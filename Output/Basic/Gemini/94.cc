#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-extension-header.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  Time interval = Seconds(1.0);
  uint8_t tclassValue = 0;
  uint8_t hopLimitValue = 255;

  CommandLine cmd;
  cmd.AddValue("packetSize", "Size of the packets sent", packetSize);
  cmd.AddValue("numPackets", "Number of packets sent", numPackets);
  cmd.AddValue("interval", "Interval between packets", interval);
  cmd.AddValue("tclass", "IPv6 TCLASS value", tclassValue);
  cmd.AddValue("hopLimit", "IPv6 Hop Limit value", hopLimitValue);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1,1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
  echoClient.SetAttribute("Interval", TimeValue(interval));
  echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> source = Socket::CreateSocket(nodes.Get(0), tid);
  Ipv6Socket::SetTclass(source, tclassValue);
  source->SetIpTtl(hopLimitValue);

  Ptr<Socket> sink = Socket::CreateSocket(nodes.Get(1), tid);
  sink->SetRecvTclass(true);
  sink->SetRecvHopLimit(true);

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}