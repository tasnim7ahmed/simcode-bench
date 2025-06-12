#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lora-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LoraWanMinimal");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  LogComponentEnable ("LoraWanMinimal", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  Ptr<LoraPhy> phy0 = CreateObject<LoraPhy> ();
  Ptr<LoraPhy> phy1 = CreateObject<LoraPhy> ();

  phy0->SetDeviceType (LoraPhy::SENSORNODE);
  phy1->SetDeviceType (LoraPhy::GATEWAY);

  Ptr<LoraChannel> channel = CreateObject<LoraChannel> ();
  channel->SetPropagationDelayModel (CreateObject<ConstantSpeedPropagationDelayModel> ());
  channel->SetPropagationLossModel (CreateObject<LogDistancePropagationLossModel> ());

  phy0->SetChannel (channel);
  phy1->SetChannel (channel);

  phy0->SetTxPower (14);
  phy1->SetTxPower (14);

  LoraNetDeviceHelper loraNetDeviceHelper;
  NetDeviceContainer devices = loraNetDeviceHelper.Install (nodes, phy0, phy1);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}