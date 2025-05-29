#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeRouterNetwork");

int main (int argc, char *argv[])
{
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable or disable tracing", tracing);
  cmd.Parse (argc, argv);

  if (tracing)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer A_B_Devices;
  A_B_Devices = pointToPoint.Install (nodes.Get (0), nodes.Get (1));

  NetDeviceContainer B_C_Devices;
  B_C_Devices = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  CsmaHelper csmaA;
  NetDeviceContainer csmaA_Devices;
  csmaA.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csmaA.SetChannelAttribute ("Delay", StringValue ("2ms"));
  csmaA_Devices = csmaA.Install (nodes.Get (0));

  CsmaHelper csmaC;
  NetDeviceContainer csmaC_Devices;
  csmaC.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csmaC.SetChannelAttribute ("Delay", StringValue ("2ms"));
  csmaC_Devices = csmaC.Install (nodes.Get (2));

  InternetStackHelper stack;
  stack.InstallAll ();

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer A_B_Interfaces = address.Assign (A_B_Devices);

  address.SetBase ("10.1.2.0", "255.255.255.252");
  Ipv4InterfaceContainer B_C_Interfaces = address.Assign (B_C_Devices);

  address.SetBase ("172.16.1.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaA_Interfaces = address.Assign (csmaA_Devices);

  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaC_Interfaces = address.Assign (csmaC_Devices);

  Ipv4Address a_addr ("1.1.1.1");
  Ipv4Address c_addr ("3.3.3.3");
  Ipv4InterfaceAddress a_iface_addr = Ipv4InterfaceAddress (a_addr, Ipv4Mask ("/32"));
  Ipv4InterfaceAddress c_iface_addr = Ipv4InterfaceAddress (c_addr, Ipv4Mask ("/32"));

  A_B_Interfaces.GetAddress (0, 0);
  B_C_Interfaces.GetAddress (1, 0);

  Ptr<Ipv4> ipv4_A = nodes.Get(0)->GetObject<Ipv4>();
  int32_t interface_index_A = ipv4_A->GetInterfaceForDevice (A_B_Devices.Get (0));
  ipv4_A->AddAddress (interface_index_A, a_iface_addr);
  ipv4_A->SetMetric (interface_index_A, 1);
  ipv4_A->SetForwarding (interface_index_A, true);

   Ptr<Ipv4> ipv4_C = nodes.Get(2)->GetObject<Ipv4>();
  int32_t interface_index_C = ipv4_C->GetInterfaceForDevice (B_C_Devices.Get (1));
  ipv4_C->AddAddress (interface_index_C, c_iface_addr);
  ipv4_C->SetMetric (interface_index_C, 1);
  ipv4_C->SetForwarding (interface_index_C, true);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper client (c_addr, port);
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("MaxPackets", UintegerValue (1000));

  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (tracing)
    {
      pointToPoint.EnablePcapAll ("ThreeRouterNetwork");
      csmaA.EnablePcapAll ("ThreeRouterNetwork");
      csmaC.EnablePcapAll ("ThreeRouterNetwork");
    }
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}