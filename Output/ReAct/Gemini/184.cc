#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (160));

  uint16_t numberOfUes = 2;

  CommandLine cmd;
  cmd.AddValue ("numberOfUes", "Number of UEs in the simulation", numberOfUes);
  cmd.Parse (argc, argv);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();

  NodeContainer enbNodes;
  enbNodes.Create (1);

  NodeContainer ueNodes;
  ueNodes.Create (numberOfUes);

  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", StringValue ("100Gb/s"));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", StringValue ("0ms"));
  NetDeviceContainer remoteHostDevices;
  remoteHostDevices = p2ph.Install (remoteHostContainer, enbNodes.Get(0));

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (5000));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18000));

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice (ueNodes);

  internet.Install (ueNodes);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4h.Assign (ueLteDevs);

  Ipv4AddressHelper remoteIp;
  remoteIp.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer remoteHostIface;
  remoteHostIface = remoteIp.Assign (remoteHostDevices);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostRouting->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"), 1);

  lteHelper.Attach (ueLteDevs, enbLteDevs.Get (0));

  uint16_t dlPort = 1234;
  UdpClientHelper client (remoteHostIface.GetAddress (0), dlPort);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  client.SetAttribute ("PacketSize", UintegerValue (1472));

  ApplicationContainer clientApps = client.Install (ueNodes);
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  UdpServerHelper server (dlPort);
  ApplicationContainer serverApps = server.Install (remoteHost);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (enbNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (ueNodes);

  AnimationInterface anim ("lte-netanim.xml");
  anim.SetConstantPosition (enbNodes.Get(0), 10.0, 10.0);
  for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        anim.SetConstantPosition (ueNodes.Get(i), 20.0 + (i*5), 20.0 + (i*5));
  }
  anim.SetConstantPosition (remoteHost, 30.0, 10.0);

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}