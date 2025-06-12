#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiAggregation");

double maxTxopDuration[4] = {0.0, 0.0, 0.0, 0.0};

void
TxopCallback (std::string context, Time txop)
{
  int apIndex = context[24] - '0';
  maxTxopDuration[apIndex] = std::max(maxTxopDuration[apIndex], txop.GetSeconds());
}

int
main (int argc, char *argv[])
{
  bool enableRtsCts = false;
  double simulationTime = 10;
  double distance = 10.0;
  double txopDuration = 0.0;
  std::string phyMode ("Ht80");
  std::string rate ("HtMcs7");

  CommandLine cmd (__FILE__);
  cmd.AddValue ("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance between AP and STA", distance);
  cmd.AddValue ("txopDuration", "TXOP duration in microseconds", txopDuration);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("rate", "Wifi Tx rate", rate);
  cmd.Parse (argc, argv);

  NodeContainer apNodes;
  apNodes.Create (4);

  NodeContainer staNodes;
  staNodes.Create (4);

  YansWifiChannelHelper channel[4];
  YansWifiPhyHelper phy[4];
  WifiHelper wifi;
  WifiMacHelper mac;
  NetDeviceContainer staDevices[4];
  NetDeviceContainer apDevices[4];

  std::vector<int> channels = {36, 40, 44, 48};

  for (int i = 0; i < 4; ++i)
    {
      std::stringstream ss;
      ss << "channel-" << channels[i];
      channel[i] = YansWifiChannelHelper::Default ();
      channel[i].AddPropagationLoss ("ns3::FriisPropagationLossModel");
      phy[i] = YansWifiPhyHelper::Default ();
      phy[i].SetChannel (channel[i].Create ());
      wifi.SetStandard (WIFI_PHY_STANDARD_80211n);
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue (rate),
                                    "ControlMode", StringValue (rate));

      Ssid ssid = Ssid (ss.str());
      mac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));

      staDevices[i] = wifi.Install (phy[i], mac, staNodes.Get (i));

      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid),
                   "BeaconGeneration", BooleanValue (true),
                   "BeaconInterval", TimeValue (Seconds (0.1)));
      apDevices[i] = wifi.Install (phy[i], mac, apNodes.Get (i));
    }

  if (enableRtsCts)
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("0"));
    }

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (10.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNodes);

  for (int i = 0; i < 4; ++i)
    {
      Ptr<ConstantPositionMobilityModel> staMobility = staNodes.Get (i)->GetObject<ConstantPositionMobilityModel> ();
      Vector3 position(distance, 0.0, 0.0);
      staMobility->SetPosition (position);
    }

  InternetStackHelper internet;
  internet.Install (apNodes);
  internet.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterfaces[4];
  Ipv4InterfaceContainer apNodeInterfaces[4];

  for (int i = 0; i < 4; ++i) {
      staNodeInterfaces[i] = address.Assign (staDevices[i]);
      apNodeInterfaces[i] = address.Assign (apDevices[i]);
      address.NewNetwork ();
  }

  uint16_t port = 9;

  UdpEchoServerHelper echoServer (port);

  ApplicationContainer serverApps = echoServer.Install (apNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpEchoClientHelper echoClient (apNodeInterfaces[0].GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.00001)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (staNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/EnableAmsduAggregation", BooleanValue (true));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/MaxAmsduSize", UintegerValue (8192));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/EnableAmpduAggregation", BooleanValue (true));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/MaxAmpduSize", UintegerValue (65535););

  Config::Set ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/EnableAmsduAggregation", BooleanValue (false));
  Config::Set ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/EnableAmpduAggregation", BooleanValue (false));

  Config::Set ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/EnableAmpduAggregation", BooleanValue (false));
  Config::Set ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/EnableAmsduAggregation", BooleanValue (true));
  Config::Set ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/MaxAmsduSize", UintegerValue (8192));

  Config::Set ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/EnableAmpduAggregation", BooleanValue (true));
  Config::Set ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/MaxAmpduSize", UintegerValue (32767));
  Config::Set ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/EnableAmsduAggregation", BooleanValue (true));
  Config::Set ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag/MaxAmsduSize", UintegerValue (4096));

  if (txopDuration > 0)
    {
      Config::SetDefault ("ns3::WifiMacQueue::MaxTotalTxopDuration", TimeValue (MicroSeconds (txopDuration)));
    }

  for(int i = 0; i < 4; ++i) {
      std::stringstream ss;
      ss << "/NodeList/" << apNodes.Get(i)->GetId() << "/DeviceList/*/$ns3::WifiNetDevice/Mac/PhyEntities/0/PhyTxBegin";
      Config::Connect (ss.str(), MakeCallback (&TxopCallback));
  }

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationAddress == apNodeInterfaces[0].GetAddress (1))
        {
            std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
            std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
            std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps\n";
        }
    }

    for (int i = 0; i < 4; ++i) {
        std::cout << "Network " << i << " Max TXOP Duration: " << maxTxopDuration[i] << " seconds" << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}