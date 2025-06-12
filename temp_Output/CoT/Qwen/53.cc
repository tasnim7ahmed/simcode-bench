#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wi-FiAggregationTest");

class TxopDurationCallback
{
public:
  void TraceTxopDuration (Time duration, Mac48Address address)
  {
    std::map<Mac48Address, Time>::iterator it = m_maxTxop.find (address);
    if (it == m_maxTxop.end () || duration > it->second)
      {
        m_maxTxop[address] = duration;
      }
  }

  void PrintResults ()
  {
    for (const auto& entry : m_maxTxop)
      {
        NS_LOG_UNCOND ("AP " << entry.first << ": Max TXOP Duration = " << entry.second.As (Time::MS));
      }
  }

private:
  std::map<Mac48Address, Time> m_maxTxop;
};

int main (int argc, char *argv[])
{
  uint32_t payloadSize = 1472; // bytes
  double simulationTime = 10.0; // seconds
  double distance = 1.0; // meters
  bool enableRtsCts = false;
  Time txopLimitA = MicroSeconds (65535); // Default maximum
  Time txopLimitB = MicroSeconds (65535);
  Time txopLimitC = MicroSeconds (65535);
  Time txopLimitD = MicroSeconds (65535);

  CommandLine cmd (__FILE__);
  cmd.AddValue ("distance", "Distance between AP and STA (m)", distance);
  cmd.AddValue ("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
  cmd.AddValue ("txopLimitA", "TXOP limit for Network A in microseconds", txopLimitA);
  cmd.AddValue ("txopLimitB", "TXOP limit for Network B in microseconds", txopLimitB);
  cmd.AddValue ("txopLimitC", "TXOP limit for Network C in microseconds", txopLimitC);
  cmd.AddValue ("txopLimitD", "TXOP limit for Network D in microseconds", txopLimitD);
  cmd.Parse (argc, argv);

  NodeContainer apNodes[4];
  NodeContainer staNodes[4];

  for (uint8_t i = 0; i < 4; ++i)
    {
      apNodes[i].Create (1);
      staNodes[i].Create (1);
    }

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_5MHZ);

  WifiMacHelper mac;
  Ssid ssid;

  NetDeviceContainer apDevices[4];
  NetDeviceContainer staDevices[4];

  TxopDurationCallback txopCallback;

  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", BooleanValue (enableRtsCts ? 0 : 999999));

  for (uint8_t i = 0; i < 4; ++i)
    {
      // Set different channels
      uint8_t channelNumber;
      switch (i)
        {
        case 0: channelNumber = 36; break;
        case 1: channelNumber = 40; break;
        case 2: channelNumber = 44; break;
        case 3: channelNumber = 48; break;
        }
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs7"));

      // Configure MAC
      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (Ssid ("network-" + std::to_string (i))),
                   "BeaconInterval", TimeValue (MicroSeconds (102400)),
                   "EnableBeaconJitter", BooleanValue (false),
                   "QosSupported", BooleanValue (true));

      // Setup TXOP tracing
      phy.Set ("ChannelNumber", UintegerValue (channelNumber));
      phy.Set ("TxopTrace", MakeCallback (&TxopDurationCallback::TraceTxopDuration, &txopCallback));

      // Install devices
      apDevices[i] = wifi.Install (phy, mac, apNodes[i]);
      mac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (Ssid ("network-" + std::to_string (i))),
                   "ActiveProbing", BooleanValue (false),
                   "QosSupported", BooleanValue (true));

      staDevices[i] = wifi.Install (phy, mac, staNodes[i]);

      // Configure aggregation settings per network
      Ptr<NetDevice> staDev = staDevices[i].Get (0);
      Ptr<WifiNetDevice> wifiStaDev = DynamicCast<WifiNetDevice> (staDev);
      if (wifiStaDev)
        {
          Ptr<WifiMac> staMac = wifiStaDev->GetMac ();
          if (i == 0) // Station A: default A-MPDU (65kB max)
            {
              staMac->SetAttribute ("Ampdu", BooleanValue (true));
              staMac->SetAttribute ("MaxAmpduSize", UintegerValue (65535));
              staMac->SetAttribute ("Amsdu", BooleanValue (false));
            }
          else if (i == 1) // Station B: disable all aggregation
            {
              staMac->SetAttribute ("Ampdu", BooleanValue (false));
              staMac->SetAttribute ("Amsdu", BooleanValue (false));
            }
          else if (i == 2) // Station C: A-MSDU only (8kB max), no A-MPDU
            {
              staMac->SetAttribute ("Amsdu", BooleanValue (true));
              staMac->SetAttribute ("MaxAmsduSize", UintegerValue (8192));
              staMac->SetAttribute ("Ampdu", BooleanValue (false));
            }
          else if (i == 3) // Station D: both aggregations (A-MPDU 32kB, A-MSDU 4kB)
            {
              staMac->SetAttribute ("Ampdu", BooleanValue (true));
              staMac->SetAttribute ("MaxAmpduSize", UintegerValue (32768));
              staMac->SetAttribute ("Amsdu", BooleanValue (true));
              staMac->SetAttribute ("MaxAmsduSize", UintegerValue (4096));
            }
        }

      // Configure TXOP limits
      Time txopLimit;
      if (i == 0) txopLimit = txopLimitA;
      else if (i == 1) txopLimit = txopLimitB;
      else if (i == 2) txopLimit = txopLimitC;
      else if (i == 3) txopLimit = txopLimitD;

      if (txopLimit.GetMicroSeconds () > 0)
        {
          Ptr<NetDevice> apDev = apDevices[i].Get (0);
          Ptr<WifiNetDevice> wifiApDev = DynamicCast<WifiNetDevice> (apDev);
          if (wifiApDev)
            {
              Ptr<WifiMac> apMac = wifiApDev->GetMac ();
              if (apMac)
                {
                  for (auto edca : apMac->GetObject<QosTxopContainer> ()->GetAllQosTxop ())
                    {
                      edca->SetTxopLimit (txopLimit);
                    }
                }
            }
        }
    }

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (4),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNodes[0]);
  mobility.Install (apNodes[1]);
  mobility.Install (apNodes[2]);
  mobility.Install (apNodes[3]);

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (5.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (4),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.Install (staNodes[0]);
  mobility.Install (staNodes[1]);
  mobility.Install (staNodes[2]);
  mobility.Install (staNodes[3]);

  InternetStackHelper stack;
  stack.Install (apNodes[0]); stack.Install (apNodes[1]); stack.Install (apNodes[2]); stack.Install (apNodes[3]);
  stack.Install (staNodes[0]); stack.Install (staNodes[1]); stack.Install (staNodes[2]); stack.Install (staNodes[3]);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer staInterfaces[4], apInterfaces[4];

  for (uint8_t i = 0; i < 4; ++i)
    {
      address.SetBase ("10.0." + std::to_string (i) + ".0", "255.255.255.0");
      apInterfaces[i] = address.Assign (apDevices[i]);
      staInterfaces[i] = address.Assign (staDevices[i]);
    }

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps;
  for (uint8_t i = 0; i < 4; ++i)
    {
      serverApps.Add (echoServer.Install (staNodes[i]));
    }
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpEchoClientHelper echoClient;
  echoClient.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFFF));
  echoClient.SetAttribute ("Interval", TimeValue (MicroSeconds (50)));
  echoClient.SetAttribute ("RemotePort", UintegerValue (9));
  echoClient.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  ApplicationContainer clientApps;
  for (uint8_t i = 0; i < 4; ++i)
    {
      echoClient.SetAttribute ("RemoteAddress", Ipv4AddressValue (staInterfaces[i].GetAddress (0)));
      clientApps = echoClient.Install (apNodes[i]);
      clientApps.Start (Seconds (1.0));
      clientApps.Stop (Seconds (simulationTime));
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowStats> stats = monitor->GetFlowStats ();

  NS_LOG_UNCOND ("Throughput results:");
  for (std::map<FlowId, FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::string srcIp = t.sourceAddress.Get ().ToString ();
      std::string dstIp = t.destinationAddress.Get ().ToString ();

      FlowId id = i->first;
      FlowStats flow = i->second;
      double duration = flow.timeLastRxPacket.GetSeconds () - flow.timeFirstTxPacket.GetSeconds ();
      double throughput = ((flow.rxBytes * 8.0) / duration) / 1e6; // Mbps

      NS_LOG_UNCOND ("Flow ID: " << id << " (" << srcIp << " -> " << dstIp << ")"
                 << ", Throughput: " << throughput << " Mbps");
    }

  txopCallback.PrintResults ();

  Simulator::Destroy ();

  return 0;
}