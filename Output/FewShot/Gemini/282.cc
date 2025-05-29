#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(4);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 8080;
  Address serverAddress (InetSocketAddress (interfaces.GetAddress (0), port));

  TcpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  BulkSendHelper clientHelper ("ns3::TcpSocketFactory", serverAddress);
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (0));
  clientHelper.SetAttribute ("SendRate", DataRateValue (DataRate ("5Mbps")));

  ApplicationContainer clientApps1 = clientHelper.Install (nodes.Get (1));
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (10.0));

  ApplicationContainer clientApps2 = clientHelper.Install (nodes.Get (2));
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (10.0));

  ApplicationContainer clientApps3 = clientHelper.Install (nodes.Get (3));
  clientApps3.Start (Seconds (2.0));
  clientApps3.Stop (Seconds (10.0));

  csma.EnablePcap ("csma-tcp", devices, true);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}