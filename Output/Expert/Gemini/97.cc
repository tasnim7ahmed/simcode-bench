#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(4);

  Names::Add("client", nodes.Get(0));
  Names::Add("server", nodes.Get(3));

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer devices = csma.Install(nodes);
  Names::Add("CSMA", devices.Get(0));
  Names::Add("CSMA", devices.Get(1));
  Names::Add("CSMA", devices.Get(2));
  Names::Add("CSMA", devices.Get(3));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer(echoPort);
  ApplicationContainer serverApps = echoServer.Install(Names::Find<Node>("server"));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(3), echoPort);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(Names::Find<Node>("client"));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Names::Add("theNodeINeed", nodes.Get(2));
  std::string tmpName = Names::FindName(nodes.Get(2));
  Names::Add("thirdNode", Names::Find<Node>("theNodeINeed"));
  Names::RemoveName("theNodeINeed");
  Config::Set("/Names/thirdNode/DeviceList/*/$ns3::CsmaNetDevice/DataRate", StringValue("10Mbps"));

  csma.EnablePcapAll("object-names");

  Simulator::Run(Seconds(10.0));
  Simulator::Destroy();
  return 0;
}