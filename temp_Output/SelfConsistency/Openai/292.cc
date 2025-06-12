#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/epc-helper.h"
#include "ns3/nr-helper.h"
#include "ns3/nr-point-to-point-epc-helper.h"
#include "ns3/nr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NrEpcSingleGnbUeExample");

int 
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Command line arguments can be added here if desired
  CommandLine cmd(__FILE__);
  cmd.Parse (argc, argv);

  // Capacity for logs (uncomment for debugging)
  //LogComponentEnable ("NrEpcSingleGnbUeExample", LOG_LEVEL_INFO);

  // 0. Create Nodes for gNB and UE
  NodeContainer ueNodes;
  NodeContainer gnbNodes;
  ueNodes.Create (1);
  gnbNodes.Create (1);

  // 1. Install Mobility
  MobilityHelper mobility;

  // gNB: stationary at (0, 0, 0)
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (gnbNodes);
  gnbNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

  // UE: starts at (10,0,0), moves at 1 m/s along X axis
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (ueNodes);
  Ptr<ConstantVelocityMobilityModel> uemob = ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  uemob->SetPosition (Vector (10.0, 0.0, 0.0));
  uemob->SetVelocity (Vector (1.0, 0.0, 0.0)); // 1 m/s along x

  // 2. Core Network + gNB/UE Internet
  Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper> ();
  Ptr<NrHelper> nrHelper = CreateObject<NrHelper> ();
  nrHelper->SetEpcHelper (epcHelper);
  nrHelper->Initialize ();

  // 3. Spectrum and PHY configuration
  double centralFrequency = 3.5e9; // 3.5 GHz, common 5G NR band
  double bandwidth = 100e6;        // 100 MHz bandwidth
  CcBwpCreator ccBwpCreator;
  const uint8_t numCcPerBand = 1;
  BandwidthPartInfoPtrVector allBwps = ccBwpCreator.Create (numCcPerBand, bandwidth, centralFrequency, nrHelper);

  nrHelper->SetGnbDeviceAttribute ("Numerology", UintegerValue(1)); // 30 kHz SCS
  nrHelper->SetGnbBwpManagerAlgorithmAttribute ("NrBwpManagerAlgorithm", StringValue ("ns3::NrBwpManagerAlgorithm"));

  // 4. Install NetDevices
  NetDeviceContainer gnbNetDev = nrHelper->InstallGnbDevice (gnbNodes, allBwps);
  NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice (ueNodes, allBwps);

  // 5. Attach UEs to the gNB
  nrHelper->AttachToClosestEnb (ueNetDev, gnbNetDev);

  // 6. Assign IP addresses to UEs
  InternetStackHelper internet;
  internet.Install (ueNodes);

  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueNetDev));
  // Enable static default routes to EPC
  epcHelper->AddDefaultEpsBearer (ueNetDev, EpsBearer (EpsBearer::NGBR_VIDEO_TCP_DEFAULT));

  // 7. Configure a remote host as the server (connect to EPC's PGW)
  Ptr<Node> remoteHost;
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create(1);
  remoteHost = remoteHostContainer.Get(0);
  InternetStackHelper internetHost;
  internetHost.Install(remoteHostContainer);

  // Connect remote host to EPC PGW
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("10Gb/s")));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.01)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIfaces = ipv4h.Assign (internetDevices);
  // Assign default route to remoteHost via PGW
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo ("7.0.0.0", "255.0.0.0", 1);

  // 8. Create UDP Server application on Remote Host (receiving packets from UE)
  uint16_t udpServerPort = 5000;
  UdpServerHelper udpServer (udpServerPort);
  ApplicationContainer serverApps = udpServer.Install (remoteHost);
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // 9. Create UDP Client application on UE (sends to remote host)
  uint32_t packetSize = 512;
  double sendInterval = 0.5; // 500 ms
  uint32_t maxPackets = static_cast<uint32_t>(10.0 / sendInterval); // 10 sec

  UdpClientHelper udpClient (internetIfaces.GetAddress (1), udpServerPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (sendInterval)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = udpClient.Install (ueNodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  // 10. Enable pcap (optional)
  //p2ph.EnablePcapAll ("nr-epc-single-gnb-ue");

  // 11. Run simulation
  Simulator::Stop (Seconds (10.01));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}