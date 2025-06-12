#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/qos-utils.h"
#include <vector>

using namespace ns3;

// Global variables for collecting statistics
struct TxopStats
{
  double totalTxop{0.0};
  uint32_t count{0};
};

std::map<uint32_t, TxopStats> g_txopStats; // key: networkId

std::vector<uint64_t> g_bytesReceived(4, 0);

// Map AC string to TID
uint8_t AcToTid (std::string ac)
{
  if (ac == "AC_VO") return 6;
  if (ac == "AC_VI") return 5;
  if (ac == "AC_BE") return 0;
  if (ac == "AC_BK") return 1;
  return 0;
}

void
TxopTrace (uint32_t networkId, uint64_t duration)
{
  g_txopStats[networkId].totalTxop += duration * 1e-6; // Convert us to seconds
  g_txopStats[networkId].count += 1;
}

void
RxPacket (Ptr<Application> app, uint32_t networkId)
{
  Ptr<UdpServer> server = DynamicCast<UdpServer> (app);
  g_bytesReceived[networkId] = server->GetReceived();
}

int
main (int argc, char *argv[])
{
  uint32_t payloadSize = 1024;
  double distance = 10.0;
  bool rtsCts = false;
  double simTime = 10.0;
  bool enablePcap = false;
  std::string phyMode ("HtMcs7");

  CommandLine cmd;
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("distance", "Distance between STA and AP nodes", distance);
  cmd.AddValue ("rtsCts", "Enable RTS/CTS", rtsCts);
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.Parse (argc, argv);

  NodeContainer staNodes[4];
  NodeContainer apNodes[4];

  for (uint32_t i=0; i<4; ++i)
    {
      staNodes[i].Create(1);
      apNodes[i].Create(1);
    }

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);

  WifiMacHelper mac;
  QosWifiMacHelper qosMac = QosWifiMacHelper::Default ();

  WifiQosSupportedFields qosFields;
  qosFields.SetQosSupportedFields (true, true, true, true);

  if (rtsCts)
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (payloadSize));
    }
  else
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (2200));
    }
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", 
    "DataMode", StringValue (phyMode),
    "ControlMode", StringValue (phyMode));

  NetDeviceContainer staDevices[4];
  NetDeviceContainer apDevices[4];
  Ssid ssids[4];

  for (uint32_t i=0; i<4; ++i)
    {
      ssids[i] = Ssid ("network-"+std::to_string(i+1));
      qosMac.SetType ("ns3::StaWifiMac",
                "Ssid", SsidValue (ssids[i]),
                "ActiveProbing", BooleanValue (false),
                "BE_MaxAmsduSize", UintegerValue (payloadSize),
                "VI_MaxAmsduSize", UintegerValue (payloadSize));
      staDevices[i] = wifi.Install (phy, qosMac, staNodes[i]);

      qosMac.SetType ("ns3::ApWifiMac",
                "Ssid", SsidValue (ssids[i]));
      apDevices[i] = wifi.Install (phy, qosMac, apNodes[i]);
    }

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  for (uint32_t i=0; i<4; ++i)
    {
      Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
      posAlloc->Add (Vector(double(i * 30), 0.0, 0.0)); // AP
      posAlloc->Add (Vector(double(i * 30 + distance), 0.0, 0.0)); // STA
      mobility.SetPositionAllocator (posAlloc);

      NodeContainer c;
      c.Add (apNodes[i].Get(0));
      c.Add (staNodes[i].Get(0));
      mobility.Install (c);
    }
  
  InternetStackHelper stack;
  for (uint32_t i=0; i<4; ++i)
    {
      stack.Install (apNodes[i]);
      stack.Install (staNodes[i]);
    }

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer staInterfaces[4], apInterfaces[4];
  for (uint32_t i=0; i<4; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i+1 << ".0";
      address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
      apInterfaces[i] = address.Assign (apDevices[i]);
      staInterfaces[i] = address.Assign (staDevices[i]);
    }

  uint16_t portBase = 9000;
  // Applications
  ApplicationContainer serverApps[4], clientApps[4];

  std::vector<std::string> acs = {"AC_BE", "AC_VI"}; // Only BE and VI
  for (uint32_t i=0; i<4; ++i)
    {
      // For each network: 2 apps per STA (BE and VI)
      for (uint32_t j=0; j<2; ++j)
        {
          uint16_t port = portBase + i*2 + j;
          UdpServerHelper server (port);
          ApplicationContainer app = server.Install (apNodes[i].Get (0));
          serverApps[i].Add (app);

          OnOffHelper client ("ns3::UdpSocketFactory", InetSocketAddress (apInterfaces[i].GetAddress (0), port));
          client.SetAttribute ("DataRate", DataRateValue (DataRate ("54Mbps")));
          client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
          client.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
          client.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      
          ApplicationContainer clientApp = client.Install (staNodes[i].Get (0));
          clientApp.Start (Seconds (1.0));
          clientApp.Stop (Seconds (simTime));
          clientApps[i].Add (clientApp);

          // Assign TOS for QoS marking (DSCP mapping)
          TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
          Ptr<Socket> sock = Socket::CreateSocket (staNodes[i].Get(0), tid);
          Ptr<Ipv4> ipv4 = staNodes[i].Get(0)->GetObject<Ipv4> ();
          Socket::SocketPriority p = Socket::NORMAL;
          if (acs[j] == "AC_VI") p = Socket::HIGH;
          sock->SetPriority (p);
          sock->Close ();
        }
    }

  // Schedule server data collection
  for (uint32_t i=0; i<4; ++i)
    {
      for (uint32_t j=0; j<2; ++j)
        {
          Simulator::Schedule (Seconds (simTime+0.001), &RxPacket, serverApps[i].Get(j), i);
        }
    }

  // PCAP
  if (enablePcap)
    {
      for(uint32_t i=0; i<4; ++i)
        {
          phy.EnablePcap ("wifiqos-ap"+std::to_string(i+1), apDevices[i], true);
          phy.EnablePcap ("wifiqos-sta"+std::to_string(i+1), staDevices[i], true);
        }
    }

  // Trace TXOP
  for (uint32_t i=0; i<4; ++i)
    {
      Ptr<NetDevice> dev = staDevices[i].Get (0);
      Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice> (dev);
      Ptr<QosTxop> txop = wifiDev->GetMac ()->GetBEQueue ()->GetTxop ();
      txop->TraceConnectWithoutContext ("TxopTrace", MakeBoundCallback (&TxopTrace, i));
      txop = wifiDev->GetMac ()->GetVIQueue ()->GetTxop ();
      txop->TraceConnectWithoutContext ("TxopTrace", MakeBoundCallback (&TxopTrace, i));
    }

  // Start and stop applications
  for (uint32_t i=0; i<4; ++i)
    {
      serverApps[i].Start (Seconds (0.0));
      serverApps[i].Stop (Seconds (simTime));
    }

  Simulator::Stop (Seconds (simTime+0.01));
  Simulator::Run ();

  // Results
  double totalSimTime = simTime - 1.0;
  for (uint32_t i=0; i<4; ++i)
    {
      uint64_t bytesReceived = g_bytesReceived[i] * payloadSize;
      double throughput = (bytesReceived * 8.0) / (totalSimTime * 1e6); // Mbps

      double txopAvg = 0.0;
      if (g_txopStats.find(i)!=g_txopStats.end() && g_txopStats[i].count>0)
        txopAvg = g_txopStats[i].totalTxop / g_txopStats[i].count;

      std::cout << "Network #" << i+1 << ": Throughput = " << throughput << " Mbps, "
                << "Avg TXOP Duration = " << txopAvg << " s" << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}