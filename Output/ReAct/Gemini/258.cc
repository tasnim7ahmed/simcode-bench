#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/ocb-wifi-mac.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::WifiMacQueue::MaxSize", StringValue ("1000"));
  Config::SetDefault ("ns3::WifiMacQueue::Mode", StringValue ("QUEUE_MODE_LIFO"));

  NodeContainer nodes;
  nodes.Create(3);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211_11P);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  QosWaveMacHelper wifiMac = QosWaveMacHelper::Default();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();

  NetDeviceContainer devices = wifi80211p.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (20.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes);

  Ptr<ConstantVelocityMobilityModel> cvmm0 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  cvmm0->SetVelocity(Vector(10,0,0));
  Ptr<ConstantVelocityMobilityModel> cvmm1 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
  cvmm1->SetVelocity(Vector(15,0,0));
  Ptr<ConstantVelocityMobilityModel> cvmm2 = nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>();
  cvmm2->SetVelocity(Vector(20,0,0));

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (i.GetAddress (2), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (3));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}