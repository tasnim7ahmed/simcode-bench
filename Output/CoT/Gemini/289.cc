#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 5000;

  TcpServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  BulkSendHelper source("ns3::TcpSocketFactory",
                         InetSocketAddress(interfaces.GetAddress(1), port));
  source.SetAttribute("SendSize", UintegerValue(1024));
  source.SetAttribute("MaxBytes", UintegerValue(0));
  source.SetAttribute("PacketRate", DataRateValue(DataRate("5Mbps")));

  ApplicationContainer sourceApps = source.Install(nodes.Get(0));
  sourceApps.Start(Seconds(2.0));
  sourceApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  pointToPoint.EnablePcapAll("tcp-bulk-send");

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}