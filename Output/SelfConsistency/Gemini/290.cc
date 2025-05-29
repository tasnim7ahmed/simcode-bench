#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LteUdpRandomWalk");

int main (int argc, char *argv[])
{
  // Set up tracing
  LogComponentEnable ("LteUdpRandomWalk", LOG_LEVEL_INFO);

  // Set simulation attributes
  uint16_t numberOfUes = 1;
  double simulationTime = 10.0;
  double interPacketInterval = 0.2; // seconds
  uint32_t packetSize = 1024;

  // Enable command line arguments
  CommandLine cmd;
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("interPacketInterval", "Inter packet interval in seconds", interPacketInterval);
  cmd.AddValue ("packetSize", "Size of packets in bytes", packetSize);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (80));

  // Create LTE helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();

  // Create eNodeB and UE nodes
  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (numberOfUes);

  // Add LTE devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Mobility model
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.Install (enbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                   "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                   "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                   "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)));
  ueMobility.Install (ueNodes);

  // Install internet stack
  InternetStackHelper internet;
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4InterfaceContainer enbIpIface;
  Ipv4InterfaceContainer ueIpIface;

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  enbIpIface = ipv4.Assign (enbLteDevs);
  ueIpIface = ipv4.Assign (ueLteDevs);

  // Attach UEs to the closest eNodeB
  lteHelper->Attach (ueLteDevs, enbLteDevs.Get (0));

  // Set active protocol
  enum EpsBearer::Qci q = EpsBearer::GBR_QCI_BEARER;

  EpsBearer bearer (q);
  lteHelper->ActivateDataRadioBearer (ueNodes, bearer);

  // Install and start applications
  uint16_t dlPort = 5000;

  // Create UdpServer application on the UE
  UdpServerHelper dlServer (dlPort);
  ApplicationContainer dlServerApps = dlServer.Install (ueNodes.Get (0));
  dlServerApps.Start (Seconds (0.0));
  dlServerApps.Stop (Seconds (simulationTime));

  // Create UdpClient application on the eNodeB
  UdpClientHelper dlClient (ueIpIface.GetAddress (0), dlPort);
  dlClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
  dlClient.SetAttribute ("Interval", TimeValue (Seconds (interPacketInterval)));
  dlClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer dlClientApps = dlClient.Install (enbNodes.Get (0));
  dlClientApps.Start (Seconds (0.0));
  dlClientApps.Stop (Seconds (simulationTime));

  // Add X2 interface
  lteHelper->AddX2Interface (enbNodes);

  // Enable PHY, MAC, and RLC tracing
  lteHelper->EnablePhyTraces ();
  lteHelper->EnableMacTraces ();
  lteHelper->EnableRlcTraces ();

  // Enable PCAP tracing
  // Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper> ("lte-udp-random-walk.pcap", std::ios::out);
  // lteHelper->EnablePcapAll (stream);

  // Run the simulation
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}