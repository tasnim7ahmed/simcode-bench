#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer enbNodes;
  enbNodes.Create (1);

  NodeContainer ueNodes;
  ueNodes.Create (3);

  // Create LTE helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  lteHelper->SetSchedulerType ("ns3::PfFfMacScheduler");

  // Install LTE devices
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Attach UEs to eNodeB
  lteHelper->Attach (ueLteDevs, enbLteDevs.Get (0));

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (ueNodes);

  // Assign IP addresses
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = lteHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

  // Set default gateway for UEs
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      Ptr<Ipv4StaticRouting> routing = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      routing->SetDefaultRoute (1, 1);
    }

  // Mobility models
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (enbNodes.Get (0));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)),
                             "Distance", DoubleValue (5.0));
  mobility.Install (ueNodes);

  // Enable PCAP tracing
  lteHelper->EnablePcap ("lte_simulation", enbLteDevs);
  lteHelper->EnablePcap ("lte_simulation", ueLteDevs);

  // UDP server on UE 0
  UdpEchoServerHelper echoServer (5000);
  ApplicationContainer serverApps = echoServer.Install (ueNodes.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // UDP client on UE 1
  UdpEchoClientHelper echoClient (ueIpIface.GetAddress (0), 5000);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (512));

  ApplicationContainer clientApps = echoClient.Install (ueNodes.Get (1));
  clientApps.Start (Seconds (0.1));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}