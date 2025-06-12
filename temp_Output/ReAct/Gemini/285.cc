#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LteUdpMobile");

int main (int argc, char *argv[])
{
  // Set default values
  uint16_t numberOfUes = 3;
  double simulationTime = 10.0;

  // Enable command line arguments parsing
  CommandLine cmd;
  cmd.AddValue ("numberOfUes", "Number of UEs in the simulation", numberOfUes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (80));

  // Create LTE Helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();

  // Create eNodeB Node
  NodeContainer enbNodes;
  enbNodes.Create (1);

  // Create UE Nodes
  NodeContainer ueNodes;
  ueNodes.Create (numberOfUes);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (enbNodes);

  mobility.SetMobilityModel ("ns3::RandomBoxPositionAllocator",
                                "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.Install (ueNodes);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Attach UE to eNodeB
  lteHelper->Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));
  lteHelper->Attach (ueLteDevs.Get (1), enbLteDevs.Get (0));
  lteHelper->Attach (ueLteDevs.Get (2), enbLteDevs.Get (0));

  // Set Active protocol to be used
  InternetStackHelper internet;
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  // Assign IP address to UEs
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign (enbLteDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign (ueLteDevs);

  // Install Applications
  uint16_t dlPort = 5000;

  // Install server on UE0
  UdpServerHelper dlServer (dlPort);
  ApplicationContainer dlServerApps = dlServer.Install (ueNodes.Get (0));
  dlServerApps.Start (Seconds (1.0));
  dlServerApps.Stop (Seconds (simulationTime - 1));

  // Install client on UE1
  UdpClientHelper dlClient (ueIpIface.GetAddress (0), dlPort);
  dlClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  dlClient.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  dlClient.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer dlClientApps = dlClient.Install (ueNodes.Get (1));
  dlClientApps.Start (Seconds (2.0));
  dlClientApps.Stop (Seconds (simulationTime - 1));

  // Activate tracing
  lteHelper->EnableTraces ();

  // Enable PCAP tracing
  Simulator::Stop (Seconds (simulationTime));
  lteHelper->EnablePcapAll ("lte-udp-mobile");

  // Run the simulation
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}