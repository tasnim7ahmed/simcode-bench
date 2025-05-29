#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(9);

  NodeContainer hubNode = NodeContainer(nodes.Get(0));
  NodeContainer spokeNodes;

  for (uint32_t i = 1; i < 9; ++i) {
    spokeNodes.Add(nodes.Get(i));
  }

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer hubDevices, spokeDevices[8];
  for (uint32_t i = 0; i < 8; ++i) {
    NodeContainer linkNodes;
    linkNodes.Add(nodes.Get(0));
    linkNodes.Add(nodes.Get(i + 1));
    NetDeviceContainer devices = p2p.Install(linkNodes);
    hubDevices.Add(devices.Get(0));
    spokeDevices[i] = NetDeviceContainer(devices.Get(1));
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer hubInterfaces, spokeInterfaces[8];
  for (uint32_t i = 0; i < 8; ++i) {
    NetDeviceContainer linkDevices;
    linkDevices.Add(hubDevices.Get(i));
    linkDevices.Add(spokeDevices[i].Get(0));
    Ipv4InterfaceContainer interfaces = address.Assign(linkDevices);
    hubInterfaces.Add(interfaces.Get(0));
    spokeInterfaces[i] = Ipv4InterfaceContainer(interfaces.Get(1));
    address.NewNetwork();
  }

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(hubNode);
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  for(uint32_t i = 0; i < 8; ++i) {
      UdpEchoClientHelper echoClient(hubInterfaces.GetAddress(i), 9);
      echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
      echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
      echoClient.SetAttribute("PacketSize", UintegerValue(1024));
      ApplicationContainer clientApps = echoClient.Install(spokeNodes.Get(i));
      clientApps.Start(Seconds(1.0));
      clientApps.Stop(Seconds(10.0));
  }

  uint16_t port = 50000;
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(hubNode);
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  for (uint32_t i = 0; i < 8; ++i) {
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", InetSocketAddress(hubInterfaces.GetAddress(i), port));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(137));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("14kbps")));
    ApplicationContainer onOffApp = onOffHelper.Install(spokeNodes.Get(i));
    onOffApp.Start(Seconds(1.0));
    onOffApp.Stop(Seconds(10.0));
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  p2p.EnablePcapAll("star");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}