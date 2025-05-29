#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WsnSimulation");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(6);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(2)));

  NetDeviceContainer csmaDevices;
  Ipv4InterfaceContainer interfaces;

  csmaDevices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  interfaces = address.Assign(csmaDevices);

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < nodes.GetN(); ++i) {
    clientApps.Add(client.Install(nodes.Get(i)));
  }

  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  AnimationInterface anim("wsn.xml");
  anim.SetConstantPosition(nodes.Get(0), 10.0, 10.0);
  anim.SetConstantPosition(nodes.Get(1), 20.0, 20.0);
  anim.SetConstantPosition(nodes.Get(2), 30.0, 20.0);
  anim.SetConstantPosition(nodes.Get(3), 20.0, 30.0);
  anim.SetConstantPosition(nodes.Get(4), 30.0, 30.0);
  anim.SetConstantPosition(nodes.Get(5), 40.0, 30.0);

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}