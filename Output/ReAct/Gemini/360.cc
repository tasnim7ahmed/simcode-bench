#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteUePhy::EnableUeMeasurements", BooleanValue (false));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType("ns3::CosineAntennaModel");
  lteHelper.SetEnbAntennaModelAttribute("Orientation", DoubleValue(0));
  lteHelper.SetEnbAntennaModelAttribute("Beamwidth", DoubleValue(60));

  PointToPointEpcHelper epcHelper;
  lteHelper.SetEpcHelper( &epcHelper );

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  InternetStackHelper internet;
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign (enbLteDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign (ueLteDevs);

  Ipv4Address enbAddr = enbIpIface.GetAddress(0);
  Ipv4Address ueAddr = ueIpIface.GetAddress(0);

  lteHelper.Attach (ueLteDevs, enbLteDevs.Get(0));

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (enbNodes.Get(0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (enbAddr, 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (10));
  ApplicationContainer clientApps = echoClient.Install (ueNodes.Get(0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}