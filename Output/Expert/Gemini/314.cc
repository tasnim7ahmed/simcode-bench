#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbNetDevice::DlEarfcn", UintegerValue (100));
  Config::SetDefault ("ns3::LteEnbNetDevice::UlEarfcn", UintegerValue (18100));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType ("ns3::IsotropicAntennaModel");

  NetDeviceContainer enbDevs;
  NetDeviceContainer ueDevs;

  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  p2pHelper.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NodeContainer epcNodes;
  epcNodes.Create(1);

  InternetStackHelper internet;
  internet.Install(epcNodes);

  NetDeviceContainer epcDevs;
  p2pHelper.Install (enbNodes.Get(0), epcNodes.Get(0), enbDevs, epcDevs);

  Ipv4AddressHelper ipAddrHelper;
  ipAddrHelper.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer epcIface = ipAddrHelper.Assign (epcDevs);

  internet.Install (enbNodes);
  internet.Install (ueNodes);

  enbDevs = lteHelper.InstallEnbDevice (enbNodes);
  ueDevs = lteHelper.InstallUeDevice (ueNodes);

  Ipv4InterfaceContainer ueIpIface;
  Ipv4InterfaceContainer enbIpIface;

  ipAddrHelper.SetBase ("10.1.2.0", "255.255.255.0");
  ueIpIface = ipAddrHelper.Assign (ueDevs);
  enbIpIface = ipAddrHelper.Assign (enbDevs);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  lteHelper.Attach (ueDevs.Get(0), enbDevs.Get(0));

  uint16_t dlPort = 9;

  UdpServerHelper dlPacketSinkHelper (dlPort);
  ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install (ueNodes.Get(0));
  dlPacketSinkApp.Start (Seconds (1.0));
  dlPacketSinkApp.Stop (Seconds (10.0));

  uint32_t packetSize = 1024;
  uint32_t numPackets = 1000;
  Time interPacketInterval = Seconds (0.01);

  UdpClientHelper dlClientHelper (ueIpIface.GetAddress(0), dlPort);
  dlClientHelper.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  dlClientHelper.SetAttribute ("Interval", TimeValue (interPacketInterval));
  dlClientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer dlClientApp = dlClientHelper.Install (enbNodes.Get(0));
  dlClientApp.Start (Seconds (2.0));
  dlClientApp.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}