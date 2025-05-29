#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BranchTopology");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetFilter("UdpClient", LOG_LEVEL_INFO);
  LogComponent::SetFilter("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices02 = p2p.Install(nodes.Get(0), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

  UdpServerHelper server(9);
  ApplicationContainer serverApp = server.Install(nodes.Get(2));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces02.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp = client.Install(nodes.Get(1));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  p2p.EnablePcapAll("branch");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}