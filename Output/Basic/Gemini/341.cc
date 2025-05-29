#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/udp-client-server-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetSimulation");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
  LogComponent::SetDefaultLogComponentEnable(true);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Mode", StringValue("Time"),
                            "Time", StringValue("2s"),
                            "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"));
  mobility.Install(nodes);

  UdpClientServerHelper udpClientServer(9);
  udpClientServer.SetAttribute("MaxPackets", UintegerValue(100));
  udpClientServer.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  udpClientServer.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = udpClientServer.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(20.0));

  ApplicationContainer serverApps = udpClientServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(20.0));

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}