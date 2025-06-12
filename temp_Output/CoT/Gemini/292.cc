#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/config-store.h"
#include "ns3/epc-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("5gNrUdp");

int main (int argc, char *argv[])
{
  Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));

  uint16_t numberOfUes = 1;
  double simTime = 10.0;
  double distance = 20.0;

  CommandLine cmd;
  cmd.AddValue ("simTime", "Total simulation time [s].", simTime);
  cmd.AddValue ("distance", "Distance between UE and gNB [m].", distance);
  cmd.Parse (argc, argv);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (numberOfUes);

  // Create gNB and UE devices
  NetDeviceContainer enbDevs;
  NetDeviceContainer ueDevs;

  enbDevs = lteHelper->InstallEnbDevice (enbNodes);
  ueDevs = lteHelper->InstallUeDevice (ueNodes);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (enbNodes);

  Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator> ();
  uePositionAlloc->Add (Vector (distance, distance, 0.0));
  mobility.SetPositionAllocator (uePositionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (ueNodes);

  ueNodes.Get (0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity (Vector (1, 1, 0));

  // Install the IP stack on the UEs
  InternetStackHelper internet;
  internet.Install (ueNodes);
  internet.Install (enbNodes);

  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueDevs));

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4h.Assign (enbDevs);

  // Attach one UE per eNB
  for (uint16_t i = 0; i < numberOfUes; i++)
    {
      lteHelper->Attach (ueDevs.Get (i), enbDevs.Get (0));
    }

  // Install and start applications on UE and eNB
  uint16_t dlPort = 2000;
  uint16_t ulPort = 3000;

  UdpClientHelper client (enbIpIface.GetAddress (0), dlPort);
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (500)));
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("PacketSize", UintegerValue (512));

  ApplicationContainer clientApps = client.Install (ueNodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simTime));

  UdpServerHelper server (dlPort);
  ApplicationContainer serverApps = server.Install (enbNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simTime));

  // Set the default gateway for the UE
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}