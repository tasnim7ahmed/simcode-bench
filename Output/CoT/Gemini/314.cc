#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LteUdpExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFirstLevel (LogComponent::ALL);
  LogComponent::Enable ("LteUdpExample");

  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (1);
  NodeContainer epcNodes;
  epcNodes.Create (1);

  Ptr<Node> enbNode = enbNodes.Get (0);
  Ptr<Node> ueNode = ueNodes.Get (0);
  Ptr<Node> epcNode = epcNodes.Get (0);

  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("100Gbps"));
  p2pHelper.SetChannelAttribute ("Delay", StringValue ("1ms"));
  NetDeviceContainer epcEnbNetDev = p2pHelper.Install (epcNode, enbNode);

  InternetStackHelper internet;
  internet.Install (epcNodes);
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4AddressHelper ip;
  ip.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer epcEnbIpIface = ip.Assign (epcEnbNetDev);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (enbNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (ueNodes);

  Ptr<ConstantPositionMobilityModel> enbMob = enbNode->GetObject<ConstantPositionMobilityModel> ();
  enbMob->SetPosition (Vector (0, 0, 0));

  Ptr<ConstantPositionMobilityModel> ueMob = ueNode->GetObject<ConstantPositionMobilityModel> ();
  ueMob->SetPosition (Vector (1, 1, 0));

  UdpServerHelper server (9);
  ApplicationContainer serverApp = server.Install (ueNodes);
  serverApp.Start (Seconds (1));
  serverApp.Stop (Seconds (10));

  UdpClientHelper client (ueNodes.Get (0)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApp = client.Install (enbNodes);
  clientApp.Start (Seconds (2));
  clientApp.Stop (Seconds (10));

  Simulator::Stop (Seconds (10));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}