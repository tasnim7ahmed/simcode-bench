#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  uint16_t numUes = 2;

  CommandLine cmd;
  cmd.AddValue ("numUes", "Number of UEs", numUes);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::LteUePhy::HarqProcessesPerTti", UintegerValue (8));

  NodeContainer enbNodes;
  enbNodes.Create (1);

  NodeContainer ueNodes;
  ueNodes.Create (numUes);

  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("5ms"));

  NetDeviceContainer remoteHostDevices;
  remoteHostDevices = pointToPoint.Install (remoteHostContainer, enbNodes.Get (0));

  InternetStackHelper internet;
  internet.Install (remoteHostContainer);
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer remoteHostInterface;
  remoteHostInterface = ipv4.Assign (remoteHostDevices);

  Ipv4Address remoteHostAddr = remoteHostInterface.GetAddress (0);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (5000));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18000));

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice (ueNodes);

  lteHelper.Attach (ueLteDevs, enbLteDevs.Get (0));

  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = lteHelper.AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ue = ueNodes.Get (u);
      Ptr<NetDevice> ueLteDev = ueLteDevs.Get (u);
      Ipv4Address ueIpAddr = ueIpIface.GetAddress (u);
      std::cout << "UE " << u << " IP Address: " << ueIpAddr << std::endl;
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t dlPort = 2000;
  UdpClientHelper client (remoteHostAddr, dlPort);
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  client.SetAttribute ("MaxPackets", UintegerValue (1000));

  ApplicationContainer clientApps;
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u) {
      clientApps.Add (client.Install (ueNodes.Get (u)));
  }

  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  UdpServerHelper server (dlPort);
  ApplicationContainer serverApps = server.Install (remoteHostContainer.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  AnimationInterface anim ("lte-netanim.xml");
  anim.SetConstantPosition (enbNodes.Get (0), 10, 10);
  anim.SetConstantPosition (remoteHostContainer.Get (0), 30, 10);
  for (uint32_t i = 0; i < ueNodes.GetN (); ++i) {
      anim.SetConstantPosition (ueNodes.Get (i), 10 + (i * 5), 20);
  }

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}