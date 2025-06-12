#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FourWifiNetsAggregationExample");

int main (int argc, char *argv[])
{
  double distance = 5.0;
  uint32_t payloadSize = 1472;
  double simTime = 10.0;
  bool rtsCtsEnabled = false;

  CommandLine cmd;
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("distance", "Distance between AP and STA", distance);
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue ("rtsCts", "Enable RTS/CTS (true/false)", rtsCtsEnabled);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  if (rtsCtsEnabled)
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("0"));
    }
  else
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
    }

  NodeContainer aps, stas;
  aps.Create (4);
  stas.Create (4);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);

  WifiMacHelper mac;
  InternetStackHelper internet;

  std::vector<NetDeviceContainer> apDevices (4), staDevices (4);
  std::vector<Ssid> ssids;

  // Aggregation parameters for each station
  std::vector<uint32_t> channelNumbers {36, 40, 44, 48};

  // A-MPDU and A-MSDU configuration per station
  std::vector<bool> mpduEnabled {true,  false, false, true};
  std::vector<bool> msduEnabled {false, false, true,  true};
  std::vector<uint32_t> mpduLimit {65535, 0, 0, 32768};
  std::vector<uint32_t> msduLimit {0, 0, 8192, 4096};

  for (uint32_t i = 0; i < 4; ++i)
    {
      std::ostringstream ss;
      ss << "wifi-net-" << (i + 1);
      ssids.push_back (Ssid (ss.str ()));

      // Configure unique channel for each network
      YansWifiPhyHelper phyNet = phy;
      phyNet.Set ("ChannelNumber", UintegerValue (channelNumbers[i]));

      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                   "DataMode", StringValue ("VhtMcs8"),
                                   "ControlMode", StringValue ("VhtMcs0"));

      // Aggregation config for AP/STA
      wifi.SetStandard (WIFI_STANDARD_80211ac);
      if (mpduEnabled[i])
        {
          Config::SetDefault ("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue (mpduLimit[i]));
        }
      else
        {
          Config::SetDefault ("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue (0));
        }
      if (msduEnabled[i])
        {
          Config::SetDefault ("ns3::WifiMac::BE_MaxAmsduSize", UintegerValue (msduLimit[i]));
        }
      else
        {
          Config::SetDefault ("ns3::WifiMac::BE_MaxAmsduSize", UintegerValue (0));
        }

      // Configure AP
      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssids[i]));
      apDevices[i] = wifi.Install (phyNet, mac, aps.Get (i));

      // Configure STA
      mac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssids[i]),
                   "ActiveProbing", BooleanValue (false));
      staDevices[i] = wifi.Install (phyNet, mac, stas.Get (i));
    }

  // Positioning
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < 4; ++i)
    {
      // AP at (i*distance*3, 0, 0), STA at (i*distance*3 + distance, 0, 0)
      positionAlloc->Add (Vector (i * distance * 3, 0.0, 0.0));
      positionAlloc->Add (Vector (i * distance * 3 + distance, 0.0, 0.0));
    }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  NodeContainer nodes;
  for (uint32_t i = 0; i < 4; ++i)
    {
      nodes.Add (aps.Get (i));
      nodes.Add (stas.Get (i));
    }
  mobility.Install (nodes);

  // Install Internet stack
  for (uint32_t i = 0; i < 4; ++i)
    {
      internet.Install (aps.Get (i));
      internet.Install (stas.Get (i));
    }

  // Assign IP addresses, use subnet 10.1.x.0/24 for network i
  std::vector<Ipv4InterfaceContainer> apIfaces (4), staIfaces (4);
  for (uint32_t i = 0; i < 4; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i+1 << ".0";
      Ipv4AddressHelper ipv4;
      ipv4.SetBase (subnet.str ().c_str (), "255.255.255.0");
      apIfaces[i] = ipv4.Assign (apDevices[i]);
      staIfaces[i] = ipv4.Assign (staDevices[i]);
    }

  // Application: each STA sends UDP to AP
  uint16_t port = 50000;
  ApplicationContainer apps;
  for (uint32_t i = 0; i < 4; ++i)
    {
      // UDP server at AP
      UdpServerHelper server (port+i);
      apps.Add (server.Install (aps.Get (i)));

      // UDP client at STA
      UdpClientHelper client (apIfaces[i].GetAddress (0), port+i);
      client.SetAttribute ("MaxPackets", UintegerValue (0));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.0)));
      client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      client.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      client.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));
      apps.Add (client.Install (stas.Get (i)));
    }

  Simulator::Stop (Seconds (simTime + 1));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}