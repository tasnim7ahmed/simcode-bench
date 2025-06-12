#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue(2));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(2);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (100));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18100));

  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", StringValue ("100Gb/s"));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", StringValue ("0ms"));

  NodeContainer epcNodes;
  epcNodes.Create (1);

  Ptr<Node> pgw = epcNodes.Get (0);

  PointToPointHelper p2pEpcs;
  p2pEpcs.SetDeviceAttribute ("DataRate", StringValue ("100Gb/s"));
  p2pEpcs.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2pEpcs.SetChannelAttribute ("Delay", StringValue ("0ms"));

  EpcHelper epcHelper;
  epcHelper.SetPgwNode (pgw);

  NetDeviceContainer enbDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueDevs = lteHelper.InstallUeDevice (ueNodes);

  InternetStackHelper internet;
  internet.Install (epcNodes);
  internet.Install (ueNodes);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer epcIface = ipv4h.Assign (p2ph.Install (NodeContainer (pgw, enbNodes.Get (0))));

  ipv4h.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIface = epcHelper.AssignUeIpv4Address (NetDeviceContainer (ueDevs));

  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ue = ueNodes.Get (u);
      Ptr<NetDevice> ueLteDev = ueDevs.Get (u);
      Ipv4Address ueIpAddr = ueIface.GetAddress (u);
      Ptr<LteUeNetDevice> lteUeDev = ueLteDev->GetObject<LteUeNetDevice> ();
      lteUeDev->SetIpv4Address (ueIpAddr);
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  lteHelper.Attach (ueDevs, enbDevs.Get (0));

  uint16_t dlPort = 10000;
  uint16_t ulPort = 20000;

  ApplicationContainer clientApps;
  ApplicationContainer serverApps;

  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ue = ueNodes.Get (u);
      Ipv4Address serverAddress = ueIface.GetAddress (1 - u);

      PacketSinkHelper dlPacketSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
      serverApps.Add (dlPacketSinkHelper.Install (ue));

      OnOffHelper dlOnOffHelper ("ns3::TcpSocketFactory", InetSocketAddress (serverAddress, dlPort));
      dlOnOffHelper.SetConstantRate (DataRate ("1Mb/s"));
      clientApps.Add (dlOnOffHelper.Install (ue));

      dlPort++;
      ulPort++;
    }

  serverApps.Start (Seconds (1.0));
  clientApps.Start (Seconds (2.0));

  Simulator::Stop (Seconds (10.0));

  AnimationInterface anim ("lte-netanim.xml");
  anim.SetConstantPosition(enbNodes.Get(0), 10, 10);
  anim.SetConstantPosition(ueNodes.Get(0), 20, 20);
  anim.SetConstantPosition(ueNodes.Get(1), 30, 30);

  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}