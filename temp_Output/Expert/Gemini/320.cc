#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (320));
  Config::SetDefault ("ns3::LteEnbRrc::SrsCyclicShift", UintegerValue (0));

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(3);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (100));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18100));
  lteHelper.SetEnbDeviceAttribute ("CellId", UintegerValue (1));

  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  p2pHelper.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2pHelper.SetChannelAttribute ("Delay", StringValue ("0ms"));

  NetDeviceContainer enbLteDevs;
  enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);

  NetDeviceContainer ueLteDevs;
  ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  InternetStackHelper internet;
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4AddressHelper ipv4Helper;
  ipv4Helper.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4Helper.Assign (enbLteDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4Helper.Assign (ueLteDevs);

  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  uint16_t port = 9;

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (enbNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper client (enbIpIface.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps;
  for(uint32_t i = 0; i < ueNodes.GetN(); ++i)
  {
      clientApps.Add (client.Install (ueNodes.Get (i)));
  }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}