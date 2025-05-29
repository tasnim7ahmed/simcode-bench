#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t numNodes = 5;

  NodeContainer nodes;
  nodes.Create(numNodes);

  NodeContainer hubNode = NodeContainer(nodes.Get(0));
  NodeContainer spokeNodes;
  for (uint32_t i = 1; i < numNodes; ++i) {
    spokeNodes.Add(nodes.Get(i));
  }

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer hubDevices, spokeDevices;

  for (uint32_t i = 1; i < numNodes; ++i) {
    NetDeviceContainer link = pointToPoint.Install(hubNode.Get(0), nodes.Get(i));
    hubDevices.Add(link.Get(0));
    spokeDevices.Add(link.Get(1));
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.9.39.0", "255.255.255.0");

  Ipv4InterfaceContainer hubInterfaces, spokeInterfaces;
  uint32_t interfaceIndex = 1;
  for (uint32_t i = 0; i < spokeNodes.GetN(); ++i) {
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(hubDevices.Get(i), spokeDevices.Get(i)));
    hubInterfaces.Add(interfaces.Get(0));
    spokeInterfaces.Add(interfaces.Get(1));
    interfaceIndex++;
  }

  UdpEchoServerHelper echoServer(9);

  ApplicationContainer serverApps[4];
  for(uint32_t i = 1; i < numNodes; ++i){
      serverApps[i-1] = echoServer.Install(nodes.Get(i));
      serverApps[i-1].Start(Seconds(1.0));
      serverApps[i-1].Stop(Seconds(10.0));
  }


  UdpEchoClientHelper echoClient(spokeInterfaces.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps[4];
  double startTime = 2.0;
  for(uint32_t i = 1; i < numNodes; ++i){
      echoClient.SetAttribute("RemoteAddress", AddressValue(spokeInterfaces.GetAddress(i-1)));
      clientApps[i-1] = echoClient.Install(nodes.Get(0));
      clientApps[i-1].Start(Seconds(startTime));
      clientApps[i-1].Stop(Seconds(startTime+3.5));
      startTime += 4.0;
  }


  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  AnimationInterface anim("star-network.xml");
  anim.SetStopAt(Seconds(30.0));
  anim.EnablePacketMetadata(true);

  Simulator::Stop(Seconds(30.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}