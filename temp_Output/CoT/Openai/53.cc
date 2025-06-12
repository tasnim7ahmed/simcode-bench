#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

// Command-line options
double g_distance = 5.0; // meters
bool g_enableRtsCts = false;
double g_txopDuration = 0.0; // microseconds; 0.0 = default

struct NetResult {
  double throughput;
  Time maxTxop;
};

NetResult results[4];

Time apTxopDur[4];
Time apCurTxop[4];

// TXOP callback
void
TxopTrace (uint32_t netIdx, Time txop)
{
  if (txop > apTxopDur[netIdx])
    apTxopDur[netIdx] = txop;
}

// Throughput callback
void
CalcThroughput (Ptr<PacketSink> sink, uint32_t netIdx)
{
  static uint64_t lastTotalRx[4] = {0,0,0,0};
  uint64_t curRx = sink->GetTotalRx ();
  double throughput = (curRx - lastTotalRx[netIdx]) * 8.0 / 1e6; // Mbps/sec
  lastTotalRx[netIdx] = curRx;
  Simulator::Schedule (Seconds(1.0), &CalcThroughput, sink, netIdx);
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.AddValue ("distance", "Distance between AP and STA in meters", g_distance);
  cmd.AddValue ("enableRtsCts", "Enable RTS/CTS", g_enableRtsCts);
  cmd.AddValue ("txopDuration", "TXOP duration in microseconds (0=default)", g_txopDuration);
  cmd.Parse (argc, argv);

  uint32_t nNets = 4;
  uint32_t nSta = 1;
  std::vector<uint16_t> channels = {36, 40, 44, 48};

  NodeContainer aps, stas;
  std::vector<NodeContainer> netAps, netStas;
  aps.Create (nNets);
  stas.Create (nNets);

  for (uint32_t i = 0; i < nNets; ++i)
    {
      netAps.push_back (NodeContainer (aps.Get(i)));
      netStas.push_back (NodeContainer (stas.Get(i)));
    }
  
  // PHY and channel setup
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  
  // Antenna and transmit power
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("TxPowerStart", DoubleValue(17));
  phy.Set ("TxPowerEnd", DoubleValue(17));
  phy.Set ("CcaMode1Threshold", DoubleValue (-62.0));
  phy.Set ("EnergyDetectionThreshold", DoubleValue (-62.0));
  
  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);

  // Use OFDM/HT
  wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager");

  std::vector<NetDeviceContainer> apDevicesVec, staDevicesVec;
 
  for (uint32_t net = 0; net < nNets; ++net)
    {
      // Different SSID/channel for each network
      std::ostringstream ssid;
      ssid << "wifi-net-" << net+1;
      Ssid ssidObj = Ssid (ssid.str ());
      // Configure channel
      phy.Set ("ChannelNumber", UintegerValue (channels[net]));
      // Configure aggregation
      WifiMacHelper mac_local = mac;
      WifiHelper wifi_local = wifi;
      NetDeviceContainer apDevice, staDevice;
      // A-MPDU/A-MSDU configuration based on net index
      if (net == 0) // net A
        {
          // Default A-MPDU, max size 65kB (~65535)
          wifi_local.SetRemoteStationManager ("ns3::MinstrelHtWifiManager",
                                              "RtsCtsThreshold", UintegerValue (g_enableRtsCts ? 0 : 65535));
          mac_local.SetType ("ns3::ApWifiMac",
                            "Ssid", SsidValue (ssidObj));
          apDevice = wifi_local.Install (phy, mac_local, netAps[net]);
          mac_local.SetType ("ns3::StaWifiMac",
                            "Ssid", SsidValue (ssidObj),
                            "ActiveProbing", BooleanValue (false));
          staDevice = wifi_local.Install (phy, mac_local, netStas[net]);
          // A-MPDU: 65kB
          Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/Ampdu", BooleanValue (true));
          Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AmpduMaxSize", UintegerValue (65535));
        }
      else if (net == 1) // net B
        {
          // Aggregation Disabled
          wifi_local.SetRemoteStationManager ("ns3::MinstrelHtWifiManager",
                                              "RtsCtsThreshold", UintegerValue (g_enableRtsCts ? 0 : 65535));
          mac_local.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssidObj));
          apDevice = wifi_local.Install (phy, mac_local, netAps[net]);
          mac_local.SetType ("ns3::StaWifiMac","Ssid", SsidValue (ssidObj),"ActiveProbing", BooleanValue (false));
          staDevice = wifi_local.Install (phy, mac_local, netStas[net]);
          // Disable all aggregation
          Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/Ampdu", BooleanValue (false));
          Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/Amsdu", BooleanValue (false));
        }
      else if (net == 2) // net C
        {
          // Enable A-MSDU only, 8kB, disable A-MPDU
          wifi_local.SetRemoteStationManager ("ns3::MinstrelHtWifiManager",
                                              "RtsCtsThreshold", UintegerValue (g_enableRtsCts ? 0 : 65535));
          mac_local.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssidObj));
          apDevice = wifi_local.Install (phy, mac_local, netAps[net]);
          mac_local.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssidObj),"ActiveProbing", BooleanValue (false));
          staDevice = wifi_local.Install (phy, mac_local, netStas[net]);
          Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/Ampdu", BooleanValue (false));
          Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/Amsdu", BooleanValue (true));
          Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AmsduMaxSize", UintegerValue (8192));
        }
      else if (net == 3) // net D
        {
          // Enable both A-MPDU 32kB and A-MSDU 4kB
          wifi_local.SetRemoteStationManager ("ns3::MinstrelHtWifiManager",
                                              "RtsCtsThreshold", UintegerValue (g_enableRtsCts ? 0 : 65535));
          mac_local.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssidObj));
          apDevice = wifi_local.Install (phy, mac_local, netAps[net]);
          mac_local.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssidObj), "ActiveProbing", BooleanValue (false));
          staDevice = wifi_local.Install (phy, mac_local, netStas[net]);
          Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/Ampdu", BooleanValue (true));
          Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AmpduMaxSize", UintegerValue (32768));
          Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/Amsdu", BooleanValue (true));
          Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AmsduMaxSize", UintegerValue (4096));
        }
      // TXOP configuration (for downlink from AP)
      if (g_txopDuration > 0.0)
        {
          for (uint32_t d = 0; d < apDevice.GetN (); ++d)
            {
              Ptr<NetDevice> dev = apDevice.Get (d);
              Ptr<WifiNetDevice> wifiNetDev = dev->GetObject<WifiNetDevice> ();
              if (wifiNetDev)
                {
                  Ptr<WifiMac> macPtr = wifiNetDev->GetMac ();
                  macPtr->SetAttribute ("TxopLimit", TimeValue (MicroSeconds (g_txopDuration)));
                }
            }
        }
      apDevicesVec.push_back (apDevice);
      staDevicesVec.push_back (staDevice);
    }

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  for (uint32_t net = 0; net < nNets; ++net)
    {
      // AP at (0,net*30,0), STA at (distance,net*30,0)
      Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
      positionAlloc->Add (Vector (0.0, 30.0 * net, 0.0));
      positionAlloc->Add (Vector (g_distance, 30.0 * net, 0.0));
      mobility.SetPositionAllocator (positionAlloc);
      NodeContainer pair;
      pair.Add (netAps[net]);
      pair.Add (netStas[net]);
      mobility.Install (pair);
    }

  // Internet stack
  InternetStackHelper stack;
  stack.Install (aps);
  stack.Install (stas);

  // IP assignment
  for (uint32_t net = 0; net < nNets; ++net)
    {
      Ipv4AddressHelper ipv4;
      std::ostringstream subnet;
      subnet << "10." << (net+1) << ".0.0";
      ipv4.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
      ipv4.Assign (apDevicesVec[net]);
      ipv4.Assign (staDevicesVec[net]);
    }

  // Set up applications (UDP traffic STA->AP, CBR)
  uint16_t port = 9000;
  ApplicationContainer sinkApps, clientApps;
  double simTime = 15.0;
  double startTime = 1.0;

  // Avoid port reuse! Use port per net
  for (uint32_t net = 0; net < nNets; ++net)
    {
      Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port+net));
      PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
      ApplicationContainer app = sinkHelper.Install (netAps[net]);
      app.Start (Seconds (startTime));
      app.Stop (Seconds (simTime));
      sinkApps.Add (app);

      Ptr<Ipv4> ipv4 = netAps[net].Get(0)->GetObject<Ipv4> ();
      Ipv4Address apAddr = ipv4->GetAddress (1,0).GetLocal ();
      UdpClientHelper clientHelper (apAddr, port+net);
      clientHelper.SetAttribute ("MaxPackets", UintegerValue (0));
      clientHelper.SetAttribute ("Interval", TimeValue (MicroSeconds (100)));
      clientHelper.SetAttribute ("PacketSize", UintegerValue (1472));
      ApplicationContainer ca = clientHelper.Install (netStas[net]);
      ca.Start (Seconds (startTime));
      ca.Stop (Seconds (simTime));
      clientApps.Add (ca);
    }

  // TXOP tracing (per AP)
  for (uint32_t net = 0; net < nNets; ++net)
    {
      Ptr<NetDevice> apDev = apDevicesVec[net].Get(0);
      Ptr<WifiNetDevice> wifiDev = apDev->GetObject<WifiNetDevice>();
      Ptr<WifiMac> mac = wifiDev->GetMac();
      Config::ConnectWithoutContext (mac->GetInstanceTypeId().GetName () + "/Txop",
        MakeBoundCallback (&TxopTrace, net));
      // Also, the correct path is needed:
      std::ostringstream txopPath;
      txopPath << "/NodeList/" << aps.Get(net)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Mac/Txop";
      Config::ConnectWithoutContext (txopPath.str (), MakeBoundCallback (&TxopTrace, net));
    }

  Simulator::Stop (Seconds (simTime + 1));
  Simulator::Run ();

  // Throughput calculation
  for (uint32_t net = 0; net < nNets; ++net)
    {
      Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApps.Get(net));
      double rxBytes = sink->GetTotalRx ();
      double throughputMbit = rxBytes * 8.0 / (simTime - startTime) / 1e6;
      results[net].throughput = throughputMbit;
      results[net].maxTxop = apTxopDur[net];
    }

  // Output results
  std::cout << std::fixed << std::setprecision(3);
  std::cout << "802.11n Aggregation Simulation Results\n";
  std::cout << "--------------------------------------------------------\n";
  std::cout << "Net\tChannel\tAggregation\tThroughput(Mbps)\tMax TXOP(us)\n";
  for (uint32_t net = 0; net < nNets; ++net)
    {
      std::string aggrType;
      if (net == 0)
        aggrType = "A-MPDU(65kB)";
      else if (net == 1)
        aggrType = "Disabled";
      else if (net == 2)
        aggrType = "A-MSDU(8kB)";
      else
        aggrType = "A-MPDU(32kB)+A-MSDU(4kB)";
      std::cout << char('A'+net) << "\t" << channels[net] << "\t" << aggrType
                << "\t" << results[net].throughput << "\t\t"
                << results[net].maxTxop.GetMicroSeconds () << "\n";
    }
  Simulator::Destroy ();
  return 0;
}