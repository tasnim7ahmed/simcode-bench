#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LteUdpExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create LTE Helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();

  // Create EPC Helper
  Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // Create Remote Host
  Ptr<Node> remoteHost = CreateObject<Node> ();
  InternetStackHelper internet;
  internet.Install (remoteHost);

  // Create EPC
  Ptr<Node> epc = epcHelper->CreateEpcNode ();

  // Connect EPC to remote host
  NetDeviceContainer remoteDevices;
  remoteDevices = epcHelper->InstallInternetStack (NetDeviceContainer ());
  remoteDevices.Add (CreateObject<PointToPointNetDevice> ()); // Dummy net device
  Ipv4AddressHelper ip;
  ip.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ip.Assign (remoteDevices);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (0);

  // Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create eNodeB
  NodeContainer enbNodes;
  enbNodes.Create (1);
  lteHelper->SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (100));
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);

  // Create UE
  NodeContainer ueNodes;
  ueNodes.Create (1);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Attach UE to eNodeB
  lteHelper->Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

  // Set active protocol
  lteHelper->ActivateDataRadioBearer (ueNodes.Get (0), EpsBearer (EpsBearer::GBR_CONV_VOICE));

  // Install applications
  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (ueNodes.Get (0));
  serverApps.Start (Seconds (1));
  serverApps.Stop (Seconds (10));

  UdpClientHelper client (internetIpIfaces.GetAddress (1), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (enbNodes.Get (0));
  clientApps.Start (Seconds (2));
  clientApps.Stop (Seconds (10));

  // Run simulation
  Simulator::Stop (Seconds (10));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}