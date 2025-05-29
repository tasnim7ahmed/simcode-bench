#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  Config::SetDefault ("ns3::LteEnbPhy::TxPower", DoubleValue(30));
  Config::SetDefault ("ns3::UdpClient::Interval", TimeValue(MilliSeconds(10)));
  Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue(1000));
  Config::SetDefault ("ns3::UdpClient::PacketSize", UintegerValue(1024));

  uint16_t numUe = 3;

  CommandLine cmd;
  cmd.AddValue ("numUe", "Number of UEs", numUe);
  cmd.Parse (argc, argv);

  NodeContainer enbNodes;
  enbNodes.Create (1);

  NodeContainer ueNodes;
  ueNodes.Create (numUe);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (100));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18100));

  NetDeviceContainer enbDevs;
  enbDevs = lteHelper.InstallEnbDevice (enbNodes);

  NetDeviceContainer ueDevs;
  ueDevs = lteHelper.InstallUeDevice (ueNodes);

  InternetStackHelper internet;
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4h.Assign (enbDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4h.Assign (ueDevs);

  lteHelper.Attach (ueDevs, enbDevs.Get (0));

  // Set mobility model
  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (0.0, 0.0, 0.0));
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator (enbPositionAlloc);
  enbMobility.Install (enbNodes);

  Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator> ();
  uePositionAlloc->Add (Vector (10.0, 10.0, 0.0));
  uePositionAlloc->Add (Vector (20.0, 20.0, 0.0));
  uePositionAlloc->Add (Vector (30.0, 30.0, 0.0));

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  ueMobility.SetPositionAllocator (uePositionAlloc);
  ueMobility.Install (ueNodes);

  uint16_t dlPort = 9;
  UdpServerHelper dlPacketSinkHelper (dlPort);
  ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install (enbNodes.Get(0));
  dlPacketSinkApp.Start (Seconds (1.0));
  dlPacketSinkApp.Stop (Seconds (10.0));

  UdpClientHelper dlClientHelper (enbIpIface.GetAddress (0), dlPort);
  ApplicationContainer dlClientApps;
  for (uint32_t u = 0; u < numUe; ++u)
    {
      dlClientApps.Add (dlClientHelper.Install (ueNodes.Get (u)));
    }
  dlClientApps.Start (Seconds (2.0));
  dlClientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}