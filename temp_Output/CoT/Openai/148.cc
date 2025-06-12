#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer centralNode;
  centralNode.Create(1);

  NodeContainer n1, n2, n3;
  n1.Add(centralNode.Get(0));
  n1.Create(1);

  n2.Add(centralNode.Get(0));
  n2.Create(1);

  n3.Add(centralNode.Get(0));
  n3.Create(1);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer d1 = p2p.Install(n1);
  NetDeviceContainer d2 = p2p.Install(n2);
  NetDeviceContainer d3 = p2p.Install(n3);

  InternetStackHelper stack;
  stack.Install(centralNode);
  stack.Install(n1.Get(1));
  stack.Install(n2.Get(1));
  stack.Install(n3.Get(1));

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i1 = address.Assign(d1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i2 = address.Assign(d2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i3 = address.Assign(d3);

  uint16_t port = 9;

  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(centralNode.Get(0));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(10.0));

  UdpClientHelper client1(i1.GetAddress(0), port);
  client1.SetAttribute("MaxPackets", UintegerValue(100000));
  client1.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
  client1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp1 = client1.Install(n1.Get(1));
  clientApp1.Start(Seconds(1.0));
  clientApp1.Stop(Seconds(10.0));

  UdpClientHelper client2(i2.GetAddress(0), port);
  client2.SetAttribute("MaxPackets", UintegerValue(100000));
  client2.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
  client2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp2 = client2.Install(n2.Get(1));
  clientApp2.Start(Seconds(1.0));
  clientApp2.Stop(Seconds(10.0));

  UdpClientHelper client3(i3.GetAddress(0), port);
  client3.SetAttribute("MaxPackets", UintegerValue(100000));
  client3.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
  client3.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp3 = client3.Install(n3.Get(1));
  clientApp3.Start(Seconds(1.0));
  clientApp3.Stop(Seconds(10.0));

  p2p.EnablePcapAll("branch-topology");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}