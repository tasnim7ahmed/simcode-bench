#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (MilliSeconds (10)));
  Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue (1000000));

  NodeContainer enbNodes;
  enbNodes.Create(2);

  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType ("ns3::IsotropicAntennaModel");

  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("100Gbps"));
  p2pHelper.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2pHelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));

  NetDeviceContainer enbNetDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueNetDevs = lteHelper.InstallUeDevice (ueNodes);

  InternetStackHelper internet;
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign (enbNetDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign (ueNetDevs);

  lteHelper.Attach (ueNetDevs.Get(0), enbNetDevs.Get(0));

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (enbNodes.Get(0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper client (enbIpIface.GetAddress (0), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (1000000));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (ueNodes.Get(0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("1.0m/s"),
                             "Bounds", RectangleValue (Rectangle (-100, 100, -100, 100)));
  mobility.Install (ueNodes.Get(0));

  Ptr<ConstantPositionMobilityModel> enb0_mobility = enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel> ();
  Ptr<ConstantPositionMobilityModel> enb1_mobility = enbNodes.Get(1)->GetObject<ConstantPositionMobilityModel> ();

  enb0_mobility->SetPosition (Vector (0.0, 0.0, 0.0));
  enb1_mobility->SetPosition (Vector (50.0, 50.0, 0.0));

  Simulator::Stop (Seconds (10));

  AnimationInterface anim ("lte-handover.xml");
  anim.SetConstantPosition (enbNodes.Get(0), 0, 0);
  anim.SetConstantPosition (enbNodes.Get(1), 50, 50);

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}