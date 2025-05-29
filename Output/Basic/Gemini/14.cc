#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/qos-txop.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiQosExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;
  uint32_t payloadSize = 1472;
  double distance = 10.0;
  bool rtsCts = false;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application containers to log if set to true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("distance", "Distance between nodes", distance);
  cmd.AddValue ("rtsCts", "Enable RTS/CTS", rtsCts);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer apNodes[4];
  NodeContainer staNodes[4];
  NetDeviceContainer apDevices[4];
  NetDeviceContainer staDevices[4];

  apNodes[0].Create (1);
  staNodes[0].Create (1);
  apNodes[1].Create (1);
  staNodes[1].Create (1);
  apNodes[2].Create (1);
  staNodes[2].Create (1);
  apNodes[3].Create (1);
  staNodes[3].Create (1);

  WifiHelper wifi[4];
  wifi[0].SetStandard (WIFI_PHY_STANDARD_80211n);
  wifi[1].SetStandard (WIFI_PHY_STANDARD_80211n);
  wifi[2].SetStandard (WIFI_PHY_STANDARD_80211n);
  wifi[3].SetStandard (WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel[4];
  YansWifiPhyHelper phy[4];

  channel[0] = YansWifiChannelHelper::Default ();
  channel[1] = YansWifiChannelHelper::Default ();
  channel[2] = YansWifiChannelHelper::Default ();
  channel[3] = YansWifiChannelHelper::Default ();

  phy[0] = YansWifiPhyHelper::Default ();
  phy[1] = YansWifiPhyHelper::Default ();
  phy[2] = YansWifiPhyHelper::Default ();
  phy[3] = YansWifiPhyHelper::Default ();

  phy[0].SetChannel (channel[0].Create ());
  phy[1].SetChannel (channel[1].Create ());
  phy[2].SetChannel (channel[2].Create ());
  phy[3].SetChannel (channel[3].Create ());

  WifiMacHelper mac[4];
  Ssid ssid[4];
  ssid[0] = Ssid ("ns-3-ssid-0");
  ssid[1] = Ssid ("ns-3-ssid-1");
  ssid[2] = Ssid ("ns-3-ssid-2");
  ssid[3] = Ssid ("ns-3-ssid-3");

  mac[0].SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid[0]),
                   "BeaconGeneration", BooleanValue (true),
                   "BeaconInterval", TimeValue (Seconds (1.0)));
  mac[1].SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid[1]),
                   "BeaconGeneration", BooleanValue (true),
                   "BeaconInterval", TimeValue (Seconds (1.0)));
    mac[2].SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid[2]),
                   "BeaconGeneration", BooleanValue (true),
                   "BeaconInterval", TimeValue (Seconds (1.0)));
    mac[3].SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid[3]),
                   "BeaconGeneration", BooleanValue (true),
                   "BeaconInterval", TimeValue (Seconds (1.0)));

  apDevices[0] = wifi[0].Install (phy[0], mac[0], apNodes[0]);
  apDevices[1] = wifi[1].Install (phy[1], mac[1], apNodes[1]);
  apDevices[2] = wifi[2].Install (phy[2], mac[2], apNodes[2]);
  apDevices[3] = wifi[3].Install (phy[3], mac[3], apNodes[3]);

  mac[0].SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid[0]),
                   "ActiveProbing", BooleanValue (false));
  mac[1].SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid[1]),
                   "ActiveProbing", BooleanValue (false));
  mac[2].SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid[2]),
                   "ActiveProbing", BooleanValue (false));
  mac[3].SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid[3]),
                   "ActiveProbing", BooleanValue (false));

  staDevices[0] = wifi[0].Install (phy[0], mac[0], staNodes[0]);
  staDevices[1] = wifi[1].Install (phy[1], mac[1], staNodes[1]);
  staDevices[2] = wifi[2].Install (phy[2], mac[2], staNodes[2]);
  staDevices[3] = wifi[3].Install (phy[3], mac[3], staNodes[3]);

  if (rtsCts)
    {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Edca/AdhocEdca/Aifs[0]", UintegerValue (3));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Edca/AdhocEdca/CwMin[0]", UintegerValue (15));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Edca/AdhocEdca/CwMax[0]", UintegerValue (1023));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Edca/AdhocEdca/Aifs[3]", UintegerValue (7));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Edca/AdhocEdca/CwMin[3]", UintegerValue (15));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Edca/AdhocEdca/CwMax[3]", UintegerValue (31));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", UintegerValue (0));
    }

  MobilityHelper mobility[4];
  mobility[0].SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (0.0),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (distance),
                                     "DeltaY", DoubleValue (0.0),
                                     "GridWidth", UintegerValue (1),
                                     "LayoutType", StringValue ("RowFirst"));

  mobility[1].SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (2*distance),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (distance),
                                     "DeltaY", DoubleValue (0.0),
                                     "GridWidth", UintegerValue (1),
                                     "LayoutType", StringValue ("RowFirst"));
  mobility[2].SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (4*distance),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (distance),
                                     "DeltaY", DoubleValue (0.0),
                                     "GridWidth", UintegerValue (1),
                                     "LayoutType", StringValue ("RowFirst"));
    mobility[3].SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (6*distance),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (distance),
                                     "DeltaY", DoubleValue (0.0),
                                     "GridWidth", UintegerValue (1),
                                     "LayoutType", StringValue ("RowFirst"));

  mobility[0].SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility[1].SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility[2].SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility[3].SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility[0].Install (apNodes[0]);
  mobility[0].Install (staNodes[0]);
  mobility[1].Install (apNodes[1]);
  mobility[1].Install (staNodes[1]);
  mobility[2].Install (apNodes[2]);
  mobility[2].Install (staNodes[2]);
  mobility[3].Install (apNodes[3]);
  mobility[3].Install (staNodes[3]);

  InternetStackHelper internet;
  internet.Install (apNodes[0]);
  internet.Install (staNodes[0]);
  internet.Install (apNodes[1]);
  internet.Install (staNodes[1]);
  internet.Install (apNodes[2]);
  internet.Install (staNodes[2]);
  internet.Install (apNodes[3]);
  internet.Install (staNodes[3]);

  Ipv4AddressHelper address[4];
  address[0].SetBase ("10.1.1.0", "255.255.255.0");
  address[1].SetBase ("10.1.2.0", "255.255.255.0");
  address[2].SetBase ("10.1.3.0", "255.255.255.0");
  address[3].SetBase ("10.1.4.0", "255.255.255.0");

  Ipv4InterfaceContainer apInterfaces[4];
  Ipv4InterfaceContainer staInterfaces[4];

  apInterfaces[0] = address[0].Assign (apDevices[0]);
  staInterfaces[0] = address[0].Assign (staDevices[0]);
  apInterfaces[1] = address[1].Assign (apDevices[1]);
  staInterfaces[1] = address[1].Assign (staDevices[1]);
    apInterfaces[2] = address[2].Assign (apDevices[2]);
  staInterfaces[2] = address[2].Assign (staDevices[2]);
    apInterfaces[3] = address[3].Assign (apDevices[3]);
  staInterfaces[3] = address[3].Assign (staDevices[3]);

  UdpServerHelper server0 (9);
  UdpServerHelper server1 (9);
  UdpServerHelper server2 (9);
  UdpServerHelper server3 (9);
  ApplicationContainer sinkApps0 = server0.Install (apNodes[0].Get (0));
  ApplicationContainer sinkApps1 = server1.Install (apNodes[1].Get (0));
  ApplicationContainer sinkApps2 = server2.Install (apNodes[2].Get (0));
  ApplicationContainer sinkApps3 = server3.Install (apNodes[3].Get (0));
  sinkApps0.Start (Seconds (0.0));
  sinkApps0.Stop (Seconds (simulationTime + 1));
  sinkApps1.Start (Seconds (0.0));
  sinkApps1.Stop (Seconds (simulationTime + 1));
  sinkApps2.Start (Seconds (0.0));
  sinkApps2.Stop (Seconds (simulationTime + 1));
  sinkApps3.Start (Seconds (0.0));
  sinkApps3.Stop (Seconds (simulationTime + 1));

  OnOffHelper onoff0 ("ns3::UdpClient", Address (InetSocketAddress (apInterfaces[0].GetAddress (0), 9)));
  onoff0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff0.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  onoff0.SetAttribute ("DataRate", StringValue ("50Mbps"));

  OnOffHelper onoff1 ("ns3::UdpClient", Address (InetSocketAddress (apInterfaces[1].GetAddress (0), 9)));
  onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff1.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  onoff1.SetAttribute ("DataRate", StringValue ("50Mbps"));

  OnOffHelper onoff2 ("ns3::UdpClient", Address (InetSocketAddress (apInterfaces[2].GetAddress (0), 9)));
  onoff2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff2.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  onoff2.SetAttribute ("DataRate", StringValue ("50Mbps"));

  OnOffHelper onoff3 ("ns3::UdpClient", Address (InetSocketAddress (apInterfaces[3].GetAddress (0), 9)));
  onoff3.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff3.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff3.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  onoff3.SetAttribute ("DataRate", StringValue ("50Mbps"));

  ApplicationContainer clientApps0 = onoff0.Install (staNodes[0].Get (0));
  clientApps0.Start (Seconds (1.0));
  clientApps0.Stop (Seconds (simulationTime));
  ApplicationContainer clientApps1 = onoff1.Install (staNodes[1].Get (0));
  clientApps1.Start (Seconds (1.0));
  clientApps1.Stop (Seconds (simulationTime));
    ApplicationContainer clientApps2 = onoff2.Install (staNodes[2].Get (0));
  clientApps2.Start (Seconds (1.0));
  clientApps2.Stop (Seconds (simulationTime));
    ApplicationContainer clientApps3 = onoff3.Install (staNodes[3].Get (0));
  clientApps3.Start (Seconds (1.0));
  clientApps3.Stop (Seconds (simulationTime));

  // Set QoS for AC_BE (Best Effort)
  TypeId tid_ac_be = TypeId::LookupByName ("ns3::WtagHeader");
  clientApps0.Get (0)->GetObject<OnOffApplication>()->SetTypeIdHeader (tid_ac_be);
  clientApps1.Get (0)->GetObject<OnOffApplication>()->SetTypeIdHeader (tid_ac_be);
  // Set QoS for AC_VI (Video)
  TypeId tid_ac_vi = TypeId::LookupByName ("ns3::LlcSnapHeader");
   clientApps2.Get (0)->GetObject<OnOffApplication>()->SetTypeIdHeader (tid_ac_vi);
   clientApps3.Get (0)->GetObject<OnOffApplication>()->SetTypeIdHeader (tid_ac_vi);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  if (tracing)
    {
      phy[0].EnablePcap ("wifi-qos-0", apDevices[0]);
      phy[1].EnablePcap ("wifi-qos-1", apDevices[1]);
      phy[2].EnablePcap ("wifi-qos-2", apDevices[2]);
      phy[3].EnablePcap ("wifi-qos-3", apDevices[3]);
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  double throughput[4] = {0.0,0.0,0.0,0.0};
  for (auto i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationAddress == apInterfaces[0].GetAddress (0))
        {
          throughput[0] = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
          std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Throughput: " << throughput[0] << " Mbps" << std::endl;
        }
      if (t.destinationAddress == apInterfaces[1].GetAddress (0))
        {
          throughput[1] = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
          std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Throughput: " << throughput[1] << " Mbps" << std::endl;
        }
         if (t.destinationAddress == apInterfaces[2].GetAddress (0))
        {
          throughput[2] = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
          std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Throughput: " << throughput[2] << " Mbps" << std::endl;
        }
         if (t.destinationAddress == apInterfaces[3].GetAddress (0))
        {
          throughput[3] = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
          std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Throughput: " << throughput[3] << " Mbps" << std::endl;
        }
    }

  Simulator::Destroy ();
  return 0;
}