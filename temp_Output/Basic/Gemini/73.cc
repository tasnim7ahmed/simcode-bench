#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer routers;
  routers.Create(3);

  NodeContainer endDevices;
  endDevices.Create(2);

  InternetStackHelper stack;
  stack.Install(routers);
  stack.Install(endDevices);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer A_B_devices = p2p.Install(routers.Get(0), routers.Get(1));
  NetDeviceContainer B_C_devices = p2p.Install(routers.Get(1), routers.Get(2));
  NetDeviceContainer A_end_device = p2p.Install(routers.Get(0), endDevices.Get(0));
  NetDeviceContainer C_end_device = p2p.Install(routers.Get(2), endDevices.Get(1));

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer A_B_interfaces = address.Assign(A_B_devices);

  address.SetBase("10.1.1.4", "255.255.255.252");
  Ipv4InterfaceContainer B_C_interfaces = address.Assign(B_C_devices);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer A_end_interfaces = address.Assign(A_end_device);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer C_end_interfaces = address.Assign(C_end_device);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory",
                   Address(InetSocketAddress(C_end_interfaces.GetAddress(1), port)));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));
  onoff.SetAttribute("DataRate", StringValue("1Mbps"));

  ApplicationContainer apps = onoff.Install(endDevices.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory",
                         Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
  apps = sink.Install(endDevices.Get(1));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  p2p.EnablePcapAll("router", false);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}