#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));

  uint16_t numberOfEnbs = 2;
  double simTime = 10.0;
  double interEnbDistance = 50.0;

  CommandLine cmd;
  cmd.AddValue ("simTime", "Total duration of the simulation [s]", simTime);
  cmd.Parse (argc, argv);

  Config::SetGlobal ("RngRun", UintegerValue (1));

  NodeContainer enbNodes;
  enbNodes.Create (numberOfEnbs);

  NodeContainer ueNodes;
  ueNodes.Create (1);

  MobilityHelper enbMobility;
  enbMobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue (0.0),
                                    "MinY", DoubleValue (0.0),
                                    "DeltaX", DoubleValue (interEnbDistance),
                                    "DeltaY", DoubleValue (0.0),
                                    "GridWidth", UintegerValue (numberOfEnbs),
                                    "LayoutType", StringValue ("RowFirst"));
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.Install (enbNodes);

  MobilityHelper ueMobility;
  ObjectFactory pos;
  pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
  pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"));
  ueMobility.SetPositionAllocator ("ns3::RandomDirection2dMobilityModel",
                                     "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=5.0]"),
                                     "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.2]"),
                                    "Bounds", StringValue ("0 0 100 1"));
  ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                  "Mode", StringValue ("Time"),
                                  "Time", StringValue ("2s"),
                                  "Direction", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=360.0]"),
                                  "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=5.0]"));
  ueMobility.Install (ueNodes);

  Ptr<RandomWalk2dMobilityModel> walker = ueNodes.Get(0)->GetObject<RandomWalk2dMobilityModel>();
  walker->SetAttribute ("Bounds", RectangleValue (Rectangle (0,0,100,1)));

  LteHelper lteHelper;

  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (100));
  lteHelper.SetUeDeviceAttribute ("DlEarfcn", UintegerValue (100));

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice (ueNodes);

  lteHelper.Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

  InternetStackHelper internet;
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign (enbLteDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign (ueLteDevs);

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (enbNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simTime - 1));

  UdpClientHelper client (enbIpIface.GetAddress (0), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (ueNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simTime - 2));

  lteHelper.EnableTracesForUe (ueNodes.Get (0), "lte-handover");

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}