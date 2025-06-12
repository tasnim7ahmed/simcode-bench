#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoSimulation");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.SetCommandName("UdpEchoSimulation");

  uint32_t packetSize = 1024;
  std::string dataRate = "5Mbps";
  std::string delay = "2ms";
  uint32_t numPackets = 100;
  double interval = 0.01;

  cmd.AddValue("packetSize", "Size of echo packet", packetSize);
  cmd.AddValue("dataRate", "Point to point data rate", dataRate);
  cmd.AddValue("delay", "Point to point delay", delay);
  cmd.AddValue("numPackets", "Number of packets sent", numPackets);
  cmd.AddValue("interval", "Interval between packets", interval);

  cmd.Parse(argc, argv);

  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

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
  echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
  echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}