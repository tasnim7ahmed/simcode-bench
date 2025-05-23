#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

int main (int argc, char *argv[])
{
  ns3::CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  ns3::NodeContainer apNode;
  apNode.Create (1);
  ns3::NodeContainer staNodes;
  staNodes.Create (3);

  ns3::MobilityHelper mobility;

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);
  apNode.Get (0)->GetObject<ns3::ConstantPositionMobilityModel> ()->SetPosition (ns3::Vector (0.0, 0.0, 0.0));

  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", ns3::StringValue ("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"),
                                 "Y", ns3::StringValue ("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                              "Bounds", ns3::RectangleValue (ns3::Rectangle (-100, 100, -100, 100)));
  mobility.Install (staNodes);

  ns3::WifiHelper wifi;
  wifi.SetStandard (ns3::WIFI_PHY_STANDARD_80211n_5GHZ);

  ns3::YansWifiChannelHelper channel;
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss ("ns3::FriisPropagationLossModel");

  ns3::YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  ns3::NqosWifiMacHelper mac;
  ns3::Ssid ssid = ns3::Ssid ("ns3-wifi-network");

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", ns3::SsidValue (ssid),
               "BeaconInterval", ns3::TimeValue (ns3::NanoSeconds (102400)));
  ns3::NetDeviceContainer apDevices = wifi.Install (phy, mac, apNode);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", ns3::SsidValue (ssid));
  ns3::NetDeviceContainer staDevices = wifi.Install (phy, mac, staNodes);

  ns3::InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNodes);

  ns3::Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  ns3::Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
  ns3::Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  ns3::UdpEchoServerHelper echoServer (9);
  ns3::ApplicationContainer serverApps = echoServer.Install (apNode.Get (0));
  serverApps.Start (ns3::Seconds (1.0));
  serverApps.Stop (ns3::Seconds (10.0));

  ns3::UdpEchoClientHelper echoClient (apInterfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", ns3::UintegerValue (5));
  echoClient.SetAttribute ("Interval", ns3::TimeValue (ns3::Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", ns3::UintegerValue (512));
  ns3::ApplicationContainer clientApps = echoClient.Install (staNodes.Get (0));
  clientApps.Start (ns3::Seconds (2.0));
  clientApps.Stop (ns3::Seconds (10.0));

  ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  phy.EnablePcap ("wifi-sta-ap", apDevices.Get (0));
  phy.EnablePcap ("wifi-sta-ap", staDevices.Get (0));

  ns3::Simulator::Stop (ns3::Seconds (10.0));
  ns3::Simulator::Run ();
  ns3::Simulator::Destroy ();

  return 0;
}