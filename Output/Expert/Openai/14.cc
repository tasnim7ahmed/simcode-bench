#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include <vector>
#include <numeric>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiQoSMultiNetwork");

struct TxopStats
{
  std::vector<double> txopDurations;
  uint64_t bytesReceived = 0;
  Time firstRx = Seconds(0);
  Time lastRx = Seconds(0);
};

std::vector<TxopStats> txopStatsVect(4);

void
RxPacketCallback (Ptr<const Packet> packet, const Address& addr, uint16_t acIndex, uint32_t apIdx)
{
  Time now = Simulator::Now ();
  if (txopStatsVect[apIdx].firstRx == Seconds (0))
    txopStatsVect[apIdx].firstRx = now;
  txopStatsVect[apIdx].lastRx = now;
  txopStatsVect[apIdx].bytesReceived += packet->GetSize();
}

void
TxopTraceCallback (Time txopStart, Time txopEnd, uint16_t acIndex, uint32_t apIdx)
{
  NS_ASSERT (txopEnd >= txopStart);
  double duration = (txopEnd - txopStart).GetSeconds ();
  txopStatsVect[apIdx].txopDurations.push_back (duration);
}

int
main (int argc, char *argv[])
{
  uint32_t payloadSize = 1024;
  double distance = 5.0;
  bool rtsCts = false;
  double simTime = 10.0;
  bool enablePcap = false;

  CommandLine cmd;
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("distance", "Distance between STA and AP", distance);
  cmd.AddValue ("rtsCts", "Enable/Disable RTS/CTS", rtsCts);
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue ("pcap", "Enable/Disable PCAP tracing", enablePcap);
  cmd.Parse (argc, argv);

  uint32_t nNetworks = 4;

  // Store ac settings for each network
  WifiMacQueue::AccessCategory acs[4] = {
    WifiMacQueue::AC_BE,  // Network 0: AC_BE
    WifiMacQueue::AC_VI,  // Network 1: AC_VI
    WifiMacQueue::AC_BE,  // Network 2: AC_BE
    WifiMacQueue::AC_VI   // Network 3: AC_VI
  };

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  Ptr<YansWifiChannel> wifiChannel = channel.Create ();

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);

  WifiMacHelper mac;
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (wifiChannel);

  InternetStackHelper stack;
  MobilityHelper mobility;

  std::vector<NodeContainer> apNodes (nNetworks), staNodes (nNetworks);
  std::vector<NetDeviceContainer> apDevices (nNetworks), staDevices (nNetworks);
  std::vector<Ipv4InterfaceContainer> apIfaces (nNetworks), staIfaces (nNetworks);

  Ipv4AddressHelper address;
  std::vector<Ipv4Address> apIpv4 (nNetworks);

  // Ssid and channel for each network
  for (uint32_t net = 0; net < nNetworks; ++net)
    {
      apNodes[net].Create (1);
      staNodes[net].Create (1);

      // Channel number for logical networks: e.g., 36, 40, 44, 48 (5GHz)
      uint16_t channelNumber = 36 + 4 * net;

      phy.Set ("ChannelNumber", UintegerValue (channelNumber));
      phy.Set ("ShortGuardEnabled", BooleanValue (true));

      // Device MAC addresses
      Ssid ssid = Ssid ("wifi-qos-" + std::to_string(net));
      mac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false),
                   "QosSupported", BooleanValue (true)
                  );
      staDevices[net] = wifi.Install (phy, mac, staNodes[net]);

      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid),
                   "QosSupported", BooleanValue (true)
                  );
      apDevices[net] = wifi.Install (phy, mac, apNodes[net]);
    }

  // Mobility: place each AP-STA in a unique area along x
  for (uint32_t net = 0; net < nNetworks; ++net)
    {
      mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobility.Install (apNodes[net]);
      mobility.Install (staNodes[net]);
      Ptr<MobilityModel> apMob = apNodes[net].Get (0)->GetObject<MobilityModel> ();
      Ptr<MobilityModel> staMob = staNodes[net].Get (0)->GetObject<MobilityModel> ();
      // AP at (net*20, 0, 0), STA at (net*20 + distance, 0, 0)
      apMob->SetPosition (Vector (net * 20.0, 0.0, 0.0));
      staMob->SetPosition (Vector (net * 20.0 + distance, 0.0, 0.0));
    }

  // Enable RTS/CTS if required
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                      UintegerValue (rtsCts ? 0 : 2347));

  // Install protocol stack
  for (uint32_t net = 0; net < nNetworks; ++net)
    {
      stack.Install (apNodes[net]);
      stack.Install (staNodes[net]);
    }

  // Assign IP addresses to APs and STAs
  for (uint32_t net = 0; net < nNetworks; ++net)
    {
      std::string subnet = "10." + std::to_string (net+1) + ".0.0";
      address.SetBase (subnet.c_str (), "255.255.255.0");
      apIfaces[net] = address.Assign (apDevices[net]);
      staIfaces[net] = address.Assign (staDevices[net]);
      apIpv4[net] = apIfaces[net].GetAddress (0);
    }

  // UDP server (on AP), OnOff app (on STA) setup, tracing
  std::vector<uint16_t> ports (nNetworks, 9000);
  std::vector<ApplicationContainer> serverApps (nNetworks), clientApps (nNetworks);

  for (uint32_t net = 0; net < nNetworks; ++net)
    {
      // Application port
      ports[net] += net;
      UdpServerHelper server (ports[net]);
      serverApps[net] = server.Install (apNodes[net]);

      // Traffic via OnOffHelper
      OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (apIpv4[net], ports[net]));
      onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("54Mbps")));
      onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));

      // Set QoS: different AC for each station by setting VHT QoS parameters
      PointerValue ptrDev;
      staDevices[net].Get (0)->GetAttribute ("Mac", ptrDev);
      Ptr<WifiMac> staMac = ptrDev.Get<WifiMac> ();
      AcIndex ac = (AcIndex)(net % 2 + 2); // 0:VO, 1:VI, 2:BE, 3:BK. Use BE/VI per scenario.
      // No direct application mapping, but OnOffHelper is best effort, use mk from net/AC idea

      // Optionally set DSCP field in IP header (for Apps)
      onoff.SetAttribute ("QosTag", EnumValue (ac == AC_BE ? AC_BE : AC_VI));

      clientApps[net] = onoff.Install (staNodes[net]);

      // Trace TXOP duration at AP MAC (monitor per network & AC)
      Ptr<NetDevice> apDev = apDevices[net].Get (0);
      PointerValue apMacPtr;
      apDev->GetAttribute ("Mac", apMacPtr);
      Ptr<WifiMac> apMac = apMacPtr.Get<WifiMac> ();

      // Connect RX callback at AP device
      apDev->TraceConnect ("PhyRxEnd", MakeBoundCallback (&RxPacketCallback, net));

      // Trace TXOP duration (Per network)
      apMac->TraceConnectWithoutContext ("Txop", MakeBoundCallback (&TxopTraceCallback, net));
    }

  if (enablePcap)
    {
      for (uint32_t net = 0; net < nNetworks; ++net)
        {
          phy.EnablePcap ("wifi-qos-ap" + std::to_string (net), apDevices[net].Get (0));
          phy.EnablePcap ("wifi-qos-sta" + std::to_string (net), staDevices[net].Get (0));
        }
    }

  Simulator::Stop (Seconds (simTime+2.0));
  Simulator::Run ();

  // Print throughput and TXOP for each network
  for (uint32_t net = 0; net < nNetworks; ++net)
    {
      uint64_t totalRxBytes = txopStatsVect[net].bytesReceived;
      double duration = (txopStatsVect[net].lastRx - txopStatsVect[net].firstRx).GetSeconds ();
      double throughput = (duration > 0.0) ? (totalRxBytes * 8.0) / (duration * 1e6) : 0.0; // Mbps

      double avgTxop = 0.0;
      if (!txopStatsVect[net].txopDurations.empty ())
        {
          double totalTxop = std::accumulate (txopStatsVect[net].txopDurations.begin (),
                                              txopStatsVect[net].txopDurations.end (), 0.0);
          avgTxop = totalTxop / txopStatsVect[net].txopDurations.size ();
        }
      std::string acType = (acs[net] == WifiMacQueue::AC_BE) ? "AC_BE" : "AC_VI";
      std::cout << "Network " << net << " (" << acType << "):"
                << " Throughput=" << throughput << " Mbps"
                << " Avg TXOP duration=" << avgTxop * 1e3 << " ms"
                << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}