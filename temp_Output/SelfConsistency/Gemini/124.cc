#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HubVlanSimulation");

int main(int argc, char *argv[]) {

  bool enablePcap = true;

  CommandLine cmd;
  cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetFilter("UdpClient", LOG_LEVEL_INFO);
  LogComponent::SetFilter("UdpServer", LOG_LEVEL_INFO);

  NodeContainer hubNode;
  hubNode.Create(1);

  NodeContainer clientNodes;
  clientNodes.Create(3);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer hubDevices;
  NetDeviceContainer clientDevices;

  for (int i = 0; i < 3; ++i) {
    NodeContainer link;
    link.Add(clientNodes.Get(i));
    link.Add(hubNode.Get(0));
    NetDeviceContainer devices = csma.Install(link);
    clientDevices.Add(devices.Get(0));
    hubDevices.Add(devices.Get(1));
  }

  InternetStackHelper stack;
  stack.Install(clientNodes);
  stack.Install(hubNode);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer clientInterfaces;
  Ipv4InterfaceContainer hubInterfaces;

  // Subnet 1: 10.1.1.0/24
  address.SetBase("10.1.1.0", "255.255.255.0");
  clientInterfaces.Add(address.AssignOne(clientDevices.Get(0)));
  hubInterfaces.Add(address.AssignOne(hubDevices.Get(0)));

  // Subnet 2: 10.1.2.0/24
  address.SetBase("10.1.2.0", "255.255.255.0");
  clientInterfaces.Add(address.AssignOne(clientDevices.Get(1)));
  hubInterfaces.Add(address.AssignOne(hubDevices.Get(1)));

  // Subnet 3: 10.1.3.0/24
  address.SetBase("10.1.3.0", "255.255.255.0");
  clientInterfaces.Add(address.AssignOne(clientDevices.Get(2)));
  hubInterfaces.Add(address.AssignOne(hubDevices.Get(2)));


  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create UDP server on client node 0
  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(clientNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Create UDP client on client node 1, sending to client node 0
  UdpClientHelper client(clientInterfaces.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(clientNodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

    // Create UDP client on client node 2, sending to client node 0
  UdpClientHelper client2(clientInterfaces.GetAddress(0), 9);
  client2.SetAttribute("MaxPackets", UintegerValue(1000));
  client2.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  client2.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps2 = client2.Install(clientNodes.Get(2));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(10.0));


  if (enablePcap) {
    csma.EnablePcap("hub-vlan", hubDevices.Get(0), true);
  }

  AnimationInterface anim("hub-vlan.xml");
  anim.SetConstantPosition(hubNode.Get(0), 10, 10);
  anim.SetConstantPosition(clientNodes.Get(0), 0, 0);
  anim.SetConstantPosition(clientNodes.Get(1), 0, 20);
  anim.SetConstantPosition(clientNodes.Get(2), 0, 40);

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}