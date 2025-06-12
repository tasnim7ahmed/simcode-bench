#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/config.h"
#include "ns3/yans-wifi-phy.h"
#include <iostream>
#include <fstream>
#include <vector>

using namespace ns3;

double apStaDistance = 10.0;
bool enableRtsCts = false;
uint32_t txopDuration = 0;

void
TxopDurationCallback (std::string context, Time txop)
{
  std::cout << context << " TXOP Duration: " << txop.GetSeconds() << " s" << std::endl;
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.AddValue ("apStaDistance", "Distance between AP and STA (meters)", apStaDistance);
  cmd.AddValue ("enableRtsCts", "Enable RTS/CTS (0 or 1)", enableRtsCts);
  cmd.AddValue ("txopDuration", "TXOP duration (microseconds)", txopDuration);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer apNodes;
  apNodes.Create (4);

  NodeContainer staNodes;
  staNodes.Create (4);

  YansWifiChannelHelper channel[4];
  YansWifiPhyHelper phy[4];
  WifiHelper wifi[4];
  NqosWifiMacHelper mac[4];

  std::vector<NetDeviceContainer> staDevices(4);
  std::vector<NetDeviceContainer> apDevices(4);

  WifiMacHelper::ApConfiguration apConfig;

  for (int i = 0; i < 4; ++i)
    {
      std::stringstream channelNumberStream;
      channelNumberStream << (36 + i * 4);
      std::string channelNumber = channelNumberStream.str();

      channel[i] = YansWifiChannelHelper::Default ();
      channel[i].AddPropagationLoss ("ns3::FriisPropagationLossModel");
      phy[i] = YansWifiPhyHelper::Default ();
      phy[i].SetChannel (channel[i].Create ());

      wifi[i] = WifiHelper::Default ();
      wifi[i].SetStandard (WIFI_PHY_STANDARD_80211n);

      mac[i] = NqosWifiMacHelper::Default ();

      Ssid ssid = Ssid ("ns3-wifi");
      mac[i].SetType ("ns3::StaWifiMac",
                      "Ssid", SsidValue (ssid),
                      "ActiveProbing", BooleanValue (false));

      staDevices[i] = wifi[i].Install (phy[i], mac[i], staNodes.Get (i));

      mac[i].SetType ("ns3::ApWifiMac",
                      "Ssid", SsidValue (ssid),
                      "BeaconGeneration", BooleanValue (true),
                      "BeaconInterval", TimeValue (Seconds (0.1)));

      apDevices[i] = wifi[i].Install (phy[i], mac[i], apNodes.Get (i));
    }

  // Configure aggregation settings
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/AmsduEnable", BooleanValue (true));

  // Station A: Default A-MPDU (65kB max)
  // Station B: Disable aggregation
  Config::Set ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/AmpduEnable", BooleanValue (false));
  Config::Set ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/AmsduEnable", BooleanValue (false));

  // Station C: Enable A-MSDU (8kB max), disable A-MPDU
  Config::Set ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/AmpduEnable", BooleanValue (false));
  Config::Set ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/MaxAmsduSize", UintegerValue (8192));

  // Station D: Enable A-MPDU (32kB max) and A-MSDU (4kB max)
  Config::Set ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/MaxAmpduSize", UintegerValue (32768));
  Config::Set ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/MaxAmsduSize", UintegerValue (4096));

  if(enableRtsCts)
  {
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", UintegerValue (0));
  }

  if(txopDuration > 0)
  {
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/Txop/GuardInterval", TimeValue (MicroSeconds(txopDuration)));
  }

  // Mobility
  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (apNodes);

  for (int i = 0; i < 4; ++i)
    {
      Ptr<ConstantPositionMobilityModel> apMobility = apNodes.Get(i)->GetObject<ConstantPositionMobilityModel> ();
      Vector3D position = apMobility->GetPosition ();
      position.x += apStaDistance;
      mobility.Install (staNodes.Get (i));
      Ptr<ConstantPositionMobilityModel> staMobility = staNodes.Get(i)->GetObject<ConstantPositionMobilityModel> ();
      staMobility->SetPosition(position);
    }

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install (apNodes);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  std::vector<Ipv4InterfaceContainer> staInterfaces(4);
  std::vector<Ipv4InterfaceContainer> apInterfaces(4);

  for (int i = 0; i < 4; ++i)
    {
      staInterfaces[i] = address.Assign (staDevices[i]);
      apInterfaces[i] = address.Assign (apDevices[i]);
      address.NewNetwork ();
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create Applications
  uint16_t port = 9;

  ApplicationContainer sinkApps[4];
  ApplicationContainer clientApps[4];

  for(int i=0; i<4; ++i)
  {
    PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    sinkApps[i] = sink.Install (staNodes.Get (i));
    sinkApps[i].Start (Seconds (1.0));
    sinkApps[i].Stop (Seconds (10.0));

    UdpClientHelper echoClient (staInterfaces[i].GetAddress (0), port);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));
    echoClient.SetAttribute ("Interval", TimeValue (MicroSeconds (1)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
    clientApps[i] = echoClient.Install (apNodes.Get (i));
    clientApps[i].Start (Seconds (2.0));
    clientApps[i].Stop (Seconds (10.0));
  }

  // Tracing TXOP
  for(int i = 0; i < 4; ++i)
  {
    std::stringstream ss;
    ss << "/NodeList/" << apNodes.Get(i)->GetId() << "/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/TxopDuration";
    Config::Connect (ss.str(), MakeCallback (&TxopDurationCallback));
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024  << " Mbps\n";
    }

  Simulator::Destroy ();

  return 0;
}