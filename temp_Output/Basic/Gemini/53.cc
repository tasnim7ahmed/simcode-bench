#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

uint32_t g_maxTxopDuration[4] = {0, 0, 0, 0};

void
TxopTrace (std::string context, Time txop)
{
  uint32_t apIndex = std::stoi(context.substr(25,1));
  if (txop.GetMicroSeconds() > g_maxTxopDuration[apIndex])
    {
      g_maxTxopDuration[apIndex] = txop.GetMicroSeconds();
    }
}

int main (int argc, char *argv[])
{
  bool enableRtsCts = false;
  uint32_t distance = 10;
  uint32_t packetSize = 1472;
  std::string rate ("OfdmRate54Mbps");

  CommandLine cmd;
  cmd.AddValue ("enableRtsCts", "Enable or disable RTS/CTS", enableRtsCts);
  cmd.AddValue ("distance", "Distance between AP and STA", distance);
  cmd.AddValue ("packetSize", "Size of packet generated", packetSize);
  cmd.AddValue ("rate", "Rate", rate);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer apNodes[4];
  NodeContainer staNodes[4];
  NetDeviceContainer apDevices[4];
  NetDeviceContainer staDevices[4];

  WifiHelper wifi[4];
  WifiMacHelper mac[4];
  YansWifiChannelHelper channel[4];
  YansWifiPhyHelper phy[4];

  InternetStackHelper stack;
  Ipv4AddressHelper address[4];
  Ipv4InterfaceContainer apInterfaces[4];
  Ipv4InterfaceContainer staInterfaces[4];

  ApplicationContainer serverApps[4];
  ApplicationContainer clientApps[4];

  for (int i = 0; i < 4; ++i)
    {
      apNodes[i].Create (1);
      staNodes[i].Create (1);

      stack.Install (apNodes[i]);
      stack.Install (staNodes[i]);

      channel[i].SetTypeId (YansWifiChannel::GetTypeId ());
      channel[i].SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
      channel[i].AddPropagationLoss ("ns3::FriisPropagationLossModel");

      phy[i].SetChannel (channel[i].Create ());

      wifi[i].SetStandard (WIFI_PHY_STANDARD_80211n);

      mac[i].SetType ("ns3::ApWifiMac",
                       "Ssid", SsidValue (Ssid ("ns-3-ssid")));

      apDevices[i] = wifi[i].Install (phy[i], mac[i], apNodes[i]);

      mac[i].SetType ("ns3::StaWifiMac",
                       "Ssid", SsidValue (Ssid ("ns-3-ssid")),
                       "ActiveProbing", BooleanValue (false));

      staDevices[i] = wifi[i].Install (phy[i], mac[i], staNodes[i]);

      address[i].SetBase ("10.1." + std::to_string(i+1) + ".0", "255.255.255.0");
      apInterfaces[i] = address[i].Assign (apDevices[i]);
      staInterfaces[i] = address[i].Assign (staDevices[i]);
      address[i].NewNetwork ();

      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

      UdpEchoServerHelper echoServer (9);
      serverApps[i] = echoServer.Install (apNodes[i].Get (0));
      serverApps[i].Start (Seconds (1.0));
      serverApps[i].Stop (Seconds (10.0));

      UdpEchoClientHelper echoClient (apInterfaces[i].GetAddress (0), 9);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (100000));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.0001)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
      clientApps[i] = echoClient.Install (staNodes[i].Get (0));
      clientApps[i].Start (Seconds (2.0));
      clientApps[i].Stop (Seconds (10.0));
    }

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  for (int i = 0; i < 4; ++i)
    {
      mobility.Install (apNodes[i]);
      mobility.Install (staNodes[i]);

      Ptr<ConstantPositionMobilityModel> apMobility = apNodes[i].Get(0)->GetObject<ConstantPositionMobilityModel>();
      apMobility->SetPosition(Vector(distance * i, 0, 0));

      Ptr<ConstantPositionMobilityModel> staMobility = staNodes[i].Get(0)->GetObject<ConstantPositionMobilityModel>();
      staMobility->SetPosition(Vector(distance * i, distance, 0));
    }

  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue (36));
  Config::Set ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue (40));
  Config::Set ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue (44));
  Config::Set ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue (48));

  if (enableRtsCts)
    {
      Config::SetDefault ("ns3::WifiMacHeader::RtsCtsThreshold", UintegerValue (0));
    }
  else
    {
      Config::SetDefault ("ns3::WifiMacHeader::RtsCtsThreshold", UintegerValue (2200));
    }

  Config::SetDefault ("ns3::WifiMacQueue::MaxAmpduSize", UintegerValue (65535));

  Config::Set ("/NodeList/5/DeviceList/*/$ns3::WifiNetDevice/Mac/BlockAckMgr/MaxAmpduSize", UintegerValue (0));

  Config::Set ("/NodeList/6/DeviceList/*/$ns3::WifiNetDevice/Mac/BlockAckMgr/AmsduEnable", BooleanValue (true));
  Config::Set ("/NodeList/6/DeviceList/*/$ns3::WifiNetDevice/Mac/BlockAckMgr/MaxAmsduSize", UintegerValue (7935));
  Config::Set ("/NodeList/6/DeviceList/*/$ns3::WifiNetDevice/Mac/BlockAckMgr/AmpduEnable", BooleanValue (false));

  Config::Set ("/NodeList/7/DeviceList/*/$ns3::WifiNetDevice/Mac/BlockAckMgr/AmsduEnable", BooleanValue (true));
  Config::Set ("/NodeList/7/DeviceList/*/$ns3::WifiNetDevice/Mac/BlockAckMgr/MaxAmsduSize", UintegerValue (4095));
  Config::Set ("/NodeList/7/DeviceList/*/$ns3::WifiNetDevice/Mac/BlockAckMgr/MaxAmpduSize", UintegerValue (32767));

  for (int i = 0; i < 4; ++i)
    {
      std::string path = "/NodeList/" + std::to_string(2*i) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueues/Be/Txop/TxopDuration";
      Config::Connect (path, MakeCallback (&TxopTrace));
    }

  Simulator::Stop (Seconds (10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  double totalThroughput[4] = {0.0, 0.0, 0.0, 0.0};

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

      for(int j = 0; j < 4; ++j){
        if(t.sourceAddress == apInterfaces[j].GetAddress (0)){
          totalThroughput[j] = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
        }
      }
    }

  Simulator::Destroy ();

  for(int i = 0; i < 4; ++i){
      std::cout << "Network " << i << " Throughput: " << totalThroughput[i] << " Mbps" << std::endl;
      std::cout << "Network " << i << " Max TXOP Duration: " << g_maxTxopDuration[i] << " us" << std::endl;
  }

  return 0;
}