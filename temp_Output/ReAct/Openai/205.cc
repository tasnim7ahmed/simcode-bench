#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VehicularAdhocSimulation");

int main (int argc, char *argv[])
{
  uint32_t numVehicles = 4;
  double totalTime = 20.0;
  double roadLength = 200.0;
  double velocity = 15.0;
  double packetInterval = 1.0; // seconds
  uint16_t udpPort = 5000;

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer vehicles;
  vehicles.Create (numVehicles);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue ("DsssRate11Mbps"),
                               "ControlMode", StringValue ("DsssRate1Mbps"));

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, vehicles);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (50.0),
                                "DeltaX", DoubleValue (roadLength/(numVehicles-1)),
                                "DeltaY", DoubleValue (0.0),
                                "GridWidth", UintegerValue (numVehicles),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.Install (vehicles);

  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      Ptr<ConstantVelocityMobilityModel> cv = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
      cv->SetVelocity (Vector (velocity, 0.0, 0.0));
    }

  InternetStackHelper stack;
  stack.Install (vehicles);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer vehicleIfaces = address.Assign (devices);

  ApplicationContainer udpApps;
  for (uint32_t src = 0; src < numVehicles; ++src)
    {
      for (uint32_t dst = 0; dst < numVehicles; ++dst)
        {
          if (src == dst) continue;

          UdpServerHelper udpServer (udpPort + dst);
          ApplicationContainer serverApp = udpServer.Install (vehicles.Get (dst));
          serverApp.Start (Seconds (0.0));
          serverApp.Stop (Seconds (totalTime));

          UdpClientHelper udpClient (vehicleIfaces.GetAddress (dst), udpPort + dst);
          udpClient.SetAttribute ("MaxPackets", UintegerValue (uint32_t (totalTime / packetInterval)));
          udpClient.SetAttribute ("Interval", TimeValue (Seconds (packetInterval)));
          udpClient.SetAttribute ("PacketSize", UintegerValue (512));

          ApplicationContainer clientApp = udpClient.Install (vehicles.Get (src));
          clientApp.Start (Seconds (1.0));
          clientApp.Stop (Seconds (totalTime));
          udpApps.Add(serverApp);
          udpApps.Add(clientApp);
        }
    }

  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll ();

  AnimationInterface anim ("vehicular-adhoc.xml");
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      anim.UpdateNodeDescription (vehicles.Get(i), "Vehicle" + std::to_string(i+1));
      anim.UpdateNodeColor (vehicles.Get(i), 255, 0, 0);
    }

  Simulator::Stop (Seconds (totalTime));
  Simulator::Run ();

  flowmon->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats ();
  std::cout << "FlowID,SrcAddr,DestAddr,TxPackets,RxPackets,LostPackets,DelaySum(s),MeanDelay(s)\n";
  for (const auto& kv : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (kv.first);
      double meanDelay = kv.second.rxPackets ? (kv.second.delaySum.GetSeconds () / kv.second.rxPackets) : 0;
      std::cout << kv.first << ","
                << t.sourceAddress << ","
                << t.destinationAddress << ","
                << kv.second.txPackets << ","
                << kv.second.rxPackets << ","
                << kv.second.lostPackets << ","
                << kv.second.delaySum.GetSeconds () << ","
                << meanDelay << "\n";
    }

  Simulator::Destroy ();
  return 0;
}