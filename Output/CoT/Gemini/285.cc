#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <ns3/point-to-point-helper.h>

using namespace ns3;

int main(int argc, char *argv[]) {

  Config::SetDefault ("ns3::LteEnbNetDevice::DlEarfcn", UintegerValue (100));
  Config::SetDefault ("ns3::LteEnbNetDevice::UlEarfcn", UintegerValue (18100));

  uint16_t numberOfUes = 3;
  double simTime = 10.0;
  double interPacketInterval = 0.01;

  CommandLine cmd;
  cmd.AddValue ("numberOfUes", "Number of UEs in the simulation", numberOfUes);
  cmd.AddValue ("simTime", "Total duration of the simulation [s]", simTime);
  cmd.Parse (argc, argv);

  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (numberOfUes);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType ("ns3::CosineAntennaModel");

  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));

  NetDeviceContainer enbLteDevs;
  NetDeviceContainer ueLteDevs;
  enbLteDevs = lteHelper.InstallEnbDevice (enbNodes);
  ueLteDevs = lteHelper.InstallUeDevice (ueNodes);

  Ptr<Node> enbNode = enbNodes.Get (0);

  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.Install (enbNode);
  Vector enbPosition(50, 50, 0);
  Ptr<MobilityModel> enbMm = enbNode->GetObject<MobilityModel> ();
  enbMm->SetPosition(enbPosition);

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel ("ns3::RandomRectanglePositionAllocator",
                               "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                               "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  ueMobility.SetConstantPositionUpdateInterval (Seconds (0.1));
  ueMobility.Install (ueNodes);

  InternetStackHelper internet;
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4InterfaceContainer ueIpIface;
  Ipv4InterfaceContainer enbIpIface;
  Ipv4StaticRoutingHelper ipv4RoutingHelper;

  lteHelper.Attach (ueLteDevs, enbLteDevs.Get (0));

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("10.1.1.0", "255.255.255.0");
  ueIpIface = ipv4h.Assign (ueLteDevs);
  enbIpIface = ipv4h.Assign (enbLteDevs);

  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
    }

  uint16_t dlPort = 5000;

  UdpServerHelper dlPacketSinkHelper (dlPort);
  ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install (ueNodes.Get(0));
  dlPacketSinkApp.Start (Seconds (0.0));
  dlPacketSinkApp.Stop (Seconds (simTime));

  uint32_t packetSize = 512;
  UdpClientHelper ulPacketGenHelper (ueIpIface.GetAddress (0), dlPort);
  ulPacketGenHelper.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  ulPacketGenHelper.SetAttribute ("Interval", TimeValue (Seconds (interPacketInterval)));
  ulPacketGenHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer ulPacketGenApp = ulPacketGenHelper.Install (ueNodes.Get(1));
  ulPacketGenApp.Start (Seconds (1.0));
  ulPacketGenApp.Stop (Seconds (simTime));

  Simulator::Stop (Seconds (simTime));

  p2ph.EnablePcapAll ("lte-sim", false);
  lteHelper.EnableTraces ();

  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}